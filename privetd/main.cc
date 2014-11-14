// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <sysexits.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/strings/string_number_conversions.h>
#include <base/values.h>
#include <chromeos/daemons/dbus_daemon.h>
#include <chromeos/flag_helper.h>
#include <chromeos/http/http_request.h>
#include <chromeos/mime_utils.h>
#include <chromeos/syslog_logging.h>
#include <libwebserv/request.h>
#include <libwebserv/response.h>
#include <libwebserv/server.h>

#include "privetd/cloud_delegate.h"
#include "privetd/device_delegate.h"
#include "privetd/privet_handler.h"
#include "privetd/security_delegate.h"
#include "privetd/wifi_delegate.h"

namespace {

using libwebserv::Request;
using libwebserv::Response;

class Daemon : public chromeos::DBusDaemon {
 public:
  Daemon(uint16_t port_number, bool allow_empty_auth, bool enable_ping)
      : port_number_(port_number),
        allow_empty_auth_(allow_empty_auth),
        enable_ping_(enable_ping) {}

  int OnInit() override {
    int ret = DBusDaemon::OnInit();
    if (ret != EX_OK)
      return EX_OK;

    if (!web_server_.Start(port_number_))
      return EX_UNAVAILABLE;

    cloud_ = privetd::CloudDelegate::CreateDefault();
    device_ = privetd::DeviceDelegate::CreateDefault(port_number_, 0);
    security_ = privetd::SecurityDelegate::CreateDefault();
    wifi_ = privetd::WifiDelegate::CreateDefault();
    privet_handler_.reset(new privetd::PrivetHandler(
        cloud_.get(), device_.get(), security_.get(), wifi_.get()));

    // TODO(vitalybuka): Device daemons should populate supported types on boot.
    device_->AddType("camera");

    web_server_.AddHandlerCallback(
        "/privet/", "",
        base::Bind(&Daemon::PrivetRequestHandler, base::Unretained(this)));

    if (enable_ping_) {
      web_server_.AddHandlerCallback(
          "/privet/ping", chromeos::http::request_type::kGet,
          base::Bind(&Daemon::HelloWorldHandler, base::Unretained(this)));
    }

    return EX_OK;
  }

  void OnShutdown(int* return_code) override {
    web_server_.Stop();
    DBusDaemon::OnShutdown(return_code);
  }

 private:
  void PrivetRequestHandler(scoped_ptr<Request> request,
                            scoped_ptr<Response> response) {
    std::vector<std::string> auth_headers =
        request->GetHeader(chromeos::http::request_header::kAuthorization);
    std::string auth_header;
    if (!auth_headers.empty())
      auth_header = auth_headers.front();
    else if (allow_empty_auth_)
      auth_header = "Privet anonymous";
    base::DictionaryValue input;
    privet_handler_->HandleRequest(
        request->GetPath(), auth_header, input,
        base::Bind(&Daemon::PrivetResponseHandler, base::Unretained(this),
                   base::Passed(&response)));
  }

  void PrivetResponseHandler(scoped_ptr<Response> response,
                             int status,
                             const base::DictionaryValue& output) {
    if (status == chromeos::http::status_code::NotFound)
      response->ReplyWithErrorNotFound();
    else
      response->ReplyWithJson(status, &output);
  }

  void HelloWorldHandler(scoped_ptr<Request> request,
                         scoped_ptr<Response> response) {
    response->ReplyWithText(chromeos::http::status_code::Ok, "Hello, world!",
                            chromeos::mime::text::kPlain);
  }


  uint16_t port_number_;
  bool allow_empty_auth_;
  bool enable_ping_;
  std::unique_ptr<privetd::CloudDelegate> cloud_;
  std::unique_ptr<privetd::DeviceDelegate> device_;
  std::unique_ptr<privetd::SecurityDelegate> security_;
  std::unique_ptr<privetd::WifiDelegate> wifi_;
  std::unique_ptr<privetd::PrivetHandler> privet_handler_;
  libwebserv::Server web_server_;

  DISALLOW_COPY_AND_ASSIGN(Daemon);
};

}  // anonymous namespace

int main(int argc, char* argv[]) {
  DEFINE_bool(allow_empty_auth, false, "allow unauthenticated requests");
  DEFINE_bool(enable_ping, false, "enable test HTTP handler at /privet/ping");
  DEFINE_int32(port, 8080, "HTTP port to listen for requests on");
  DEFINE_bool(log_to_stderr, false, "log trace messages to stderr as well");

  chromeos::FlagHelper::Init(argc, argv, "Privet protocol handler daemon");

  int flags = chromeos::kLogToSyslog;
  if (FLAGS_log_to_stderr)
    flags |= chromeos::kLogToStderr;
  chromeos::InitLog(flags | chromeos::kLogHeader);

  if (FLAGS_port < 1 || FLAGS_port > 0xFFFF) {
    LOG(ERROR) << "Invalid port number specified: '" << FLAGS_port << "'.";
    return EX_USAGE;
  }

  Daemon daemon(FLAGS_port, FLAGS_allow_empty_auth, FLAGS_enable_ping);
  return daemon.Run();
}
