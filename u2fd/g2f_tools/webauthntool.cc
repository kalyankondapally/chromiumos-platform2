// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sysexits.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <base/macros.h>
#include <base/strings/string_number_conversions.h>
#include <brillo/dbus/dbus_method_invoker.h>
#include <brillo/dbus/dbus_object.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <dbus/u2f/dbus-constants.h>
#include <u2f/proto_bindings/u2f_interface.pb.h>

template <typename Req, typename Resp>
Resp SendRequest(dbus::ObjectProxy* proxy,
                 const std::string& method_name,
                 const Req& req) {
  brillo::ErrorPtr error;

  std::unique_ptr<dbus::Response> dbus_response =
      brillo::dbus_utils::CallMethodAndBlock(proxy, u2f::kU2FInterface,
                                             method_name, &error, req);

  if (!dbus_response) {
    LOG(FATAL) << "Call to " << method_name << " failed";
  }

  Resp resp;

  dbus::MessageReader reader(dbus_response.get());
  if (!reader.PopArrayOfBytesAsProto(&resp)) {
    LOG(FATAL) << "Failed to parse reply for call to " << method_name;
  }

  return resp;
}

std::string HexEncodeStr(const std::string& str) {
  return base::HexEncode(str.data(), str.size());
}

void AppendToString(const std::vector<uint8_t>& vect, std::string* str) {
  str->append(reinterpret_cast<const char*>(vect.data()), vect.size());
}

void MakeCredential(dbus::ObjectProxy* proxy,
                    int verification_type,
                    const std::string& rp_id) {
  u2f::MakeCredentialRequest req;
  req.set_verification_type(
      static_cast<u2f::VerificationType>(verification_type));
  req.set_rp_id(rp_id);

  if (verification_type == u2f::VERIFICATION_USER_VERIFICATION) {
    LOG(INFO) << "Please touch the fingerprint sensor.";
  } else if (verification_type == u2f::VERIFICATION_USER_PRESENCE) {
    LOG(INFO) << "Please press the power button.";
  }

  u2f::MakeCredentialResponse resp =
      SendRequest<u2f::MakeCredentialRequest, u2f::MakeCredentialResponse>(
          proxy, u2f::kU2FMakeCredential, req);

  LOG(INFO) << "status: " << resp.status();
  LOG(INFO) << "authenticator_data: "
            << HexEncodeStr(resp.authenticator_data());
  LOG(INFO) << "attestation_format: " << resp.attestation_format();
  LOG(INFO) << "attestation_statement: "
            << HexEncodeStr(resp.attestation_statement());
}

void GetAssertion(dbus::ObjectProxy* proxy,
                  int verification_type,
                  const std::string& rp_id,
                  const std::string& client_data_hash,
                  const std::string& allowed_credential_id) {
  u2f::GetAssertionRequest req;
  req.set_verification_type(
      static_cast<u2f::VerificationType>(verification_type));
  req.set_rp_id(rp_id);
  req.set_client_data_hash(client_data_hash);

  std::vector<uint8_t> credential_id_bytes;
  if (!base::HexStringToBytes(allowed_credential_id, &credential_id_bytes)) {
    LOG(FATAL) << "Could not parse credential_id bytes";
  }

  AppendToString(credential_id_bytes, req.add_allowed_credential_id());

  u2f::GetAssertionResponse resp =
      SendRequest<u2f::GetAssertionRequest, u2f::GetAssertionResponse>(
          proxy, u2f::kU2FGetAssertion, req);

  LOG(INFO) << "status: " << resp.status();
  for (const auto& assertion : resp.assertion()) {
    LOG(INFO) << "credential_id: " << HexEncodeStr(assertion.credential_id());
    LOG(INFO) << "authenticator_data: "
              << HexEncodeStr(assertion.authenticator_data());
    LOG(INFO) << "signature: " << HexEncodeStr(assertion.signature().data());
  }
}

void HasCredentials(dbus::ObjectProxy* proxy,
                    const std::string& rp_id,
                    const std::string& credential_id) {
  u2f::HasCredentialsRequest req;
  req.set_rp_id(rp_id);

  std::vector<uint8_t> credential_id_bytes;
  if (!base::HexStringToBytes(credential_id, &credential_id_bytes)) {
    LOG(FATAL) << "Could not parse credential_id bytes";
  }

  AppendToString(credential_id_bytes, req.add_credential_id());

  u2f::HasCredentialsResponse resp =
      SendRequest<u2f::HasCredentialsRequest, u2f::HasCredentialsResponse>(
          proxy, u2f::kU2FHasCredentials, req);

  LOG(INFO) << "number matched: " << resp.credential_id().size();
  for (const auto& cred : resp.credential_id()) {
    LOG(INFO) << "credential_id: " << HexEncodeStr(cred);
  }
}

int main(int argc, char* argv[]) {
  DEFINE_bool(make_credential, false, "make a credential");
  DEFINE_bool(get_assertion, false, "get an assertion");
  DEFINE_bool(has_credentials, false,
              "check validity/existence of credentials");

  DEFINE_int32(verification_type, 1,
               "type of verification to request: presence=1, verification=2");
  DEFINE_string(rp_id, "", "relaying party ID (domain name)");
  DEFINE_string(client_data_hash, "", "client data hash, as a hex string");
  DEFINE_string(credential_id, "", "list of credential IDs, as hex strings");

  brillo::FlagHelper::Init(argc, argv,
                           "webauthntool - WebAuthn DBus API testing tool");
  brillo::InitLog(brillo::kLogToStderrIfTty);

  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;

  scoped_refptr<dbus::Bus> bus = new dbus::Bus(options);

  if (!bus->Connect()) {
    LOG(ERROR) << "Cannot connect to D-Bus.";
    return EX_IOERR;
  }

  dbus::ObjectProxy* u2f_proxy = bus->GetObjectProxy(
      u2f::kU2FServiceName, dbus::ObjectPath(u2f::kU2FServicePath));

  CHECK(u2f_proxy) << "Couldn't get u2f proxy";

  if (FLAGS_make_credential) {
    MakeCredential(u2f_proxy, FLAGS_verification_type, FLAGS_rp_id);
    return EX_OK;
  }

  if (FLAGS_get_assertion) {
    GetAssertion(u2f_proxy, FLAGS_verification_type, FLAGS_rp_id,
                 FLAGS_client_data_hash, FLAGS_credential_id);
    return EX_OK;
  }

  if (FLAGS_has_credentials) {
    HasCredentials(u2f_proxy, FLAGS_rp_id, FLAGS_credential_id);
    return EX_OK;
  }

  LOG(INFO) << "Please specify a command.";

  return EX_USAGE;
}