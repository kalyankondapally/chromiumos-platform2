// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include <base/bind.h>
#include <dbus/mock_object_proxy.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <attestation/client/dbus_proxy.h>
#include <attestation-client/attestation/dbus-constants.h>

using testing::_;
using testing::Invoke;
using testing::StrictMock;
using testing::WithArgs;

namespace attestation {

class DBusProxyTest : public testing::Test {
 public:
  ~DBusProxyTest() override = default;
  void SetUp() override {
    mock_object_proxy_ = new StrictMock<dbus::MockObjectProxy>(
        nullptr, "", dbus::ObjectPath(kAttestationServicePath));
    proxy_.set_object_proxy(mock_object_proxy_.get());
  }

 protected:
  scoped_refptr<StrictMock<dbus::MockObjectProxy>> mock_object_proxy_;
  DBusProxy proxy_;
};

TEST_F(DBusProxyTest, GetKeyInfo) {
  auto fake_dbus_call =
      [](dbus::MethodCall* method_call,
         dbus::MockObjectProxy::ResponseCallback* response_callback) {
        // Verify request protobuf.
        dbus::MessageReader reader(method_call);
        GetKeyInfoRequest request_proto;
        EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
        EXPECT_EQ("label", request_proto.key_label());
        EXPECT_EQ("username", request_proto.username());
        // Create reply protobuf.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        GetKeyInfoReply reply_proto;
        reply_proto.set_status(STATUS_SUCCESS);
        reply_proto.set_key_type(KEY_TYPE_ECC);
        reply_proto.set_key_usage(KEY_USAGE_SIGN);
        reply_proto.set_public_key("public_key");
        reply_proto.set_certify_info("certify_info");
        reply_proto.set_certify_info_signature("signature");
        reply_proto.set_certificate("certificate");
        writer.AppendProtoAsArrayOfBytes(reply_proto);
        std::move(*response_callback).Run(response.get());
      };
  EXPECT_CALL(*mock_object_proxy_, DoCallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [](int* callback_count, const GetKeyInfoReply& reply) {
    (*callback_count)++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ(KEY_TYPE_ECC, reply.key_type());
    EXPECT_EQ(KEY_USAGE_SIGN, reply.key_usage());
    EXPECT_EQ("public_key", reply.public_key());
    EXPECT_EQ("certify_info", reply.certify_info());
    EXPECT_EQ("signature", reply.certify_info_signature());
    EXPECT_EQ("certificate", reply.certificate());
  };
  GetKeyInfoRequest request;
  request.set_key_label("label");
  request.set_username("username");
  proxy_.GetKeyInfo(request, base::Bind(callback, &callback_count));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, GetEndorsementInfo) {
  auto fake_dbus_call =
      [](dbus::MethodCall* method_call,
         dbus::MockObjectProxy::ResponseCallback* response_callback) {
        // Verify request protobuf.
        dbus::MessageReader reader(method_call);
        GetEndorsementInfoRequest request_proto;
        EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
        // Create reply protobuf.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        GetEndorsementInfoReply reply_proto;
        reply_proto.set_status(STATUS_SUCCESS);
        reply_proto.set_ek_public_key("public_key");
        reply_proto.set_ek_certificate("certificate");
        writer.AppendProtoAsArrayOfBytes(reply_proto);
        std::move(*response_callback).Run(response.get());
      };
  EXPECT_CALL(*mock_object_proxy_, DoCallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [](int* callback_count,
                     const GetEndorsementInfoReply& reply) {
    (*callback_count)++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("public_key", reply.ek_public_key());
    EXPECT_EQ("certificate", reply.ek_certificate());
  };
  GetEndorsementInfoRequest request;
  proxy_.GetEndorsementInfo(request, base::Bind(callback, &callback_count));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, GetAttestationKeyInfo) {
  auto fake_dbus_call =
      [](dbus::MethodCall* method_call,
         dbus::MockObjectProxy::ResponseCallback* response_callback) {
        // Verify request protobuf.
        dbus::MessageReader reader(method_call);
        GetAttestationKeyInfoRequest request_proto;
        EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
        // Create reply protobuf.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        GetAttestationKeyInfoReply reply_proto;
        reply_proto.set_status(STATUS_SUCCESS);
        reply_proto.set_public_key("public_key");
        reply_proto.set_public_key_tpm_format("public_key_tpm_format");
        reply_proto.set_certificate("certificate");
        reply_proto.mutable_pcr0_quote()->set_quote("pcr0");
        reply_proto.mutable_pcr1_quote()->set_quote("pcr1");
        writer.AppendProtoAsArrayOfBytes(reply_proto);
        std::move(*response_callback).Run(response.get());
      };
  EXPECT_CALL(*mock_object_proxy_, DoCallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [](int* callback_count,
                     const GetAttestationKeyInfoReply& reply) {
    (*callback_count)++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("public_key", reply.public_key());
    EXPECT_EQ("public_key_tpm_format", reply.public_key_tpm_format());
    EXPECT_EQ("certificate", reply.certificate());
    EXPECT_EQ("pcr0", reply.pcr0_quote().quote());
    EXPECT_EQ("pcr1", reply.pcr1_quote().quote());
  };
  GetAttestationKeyInfoRequest request;
  proxy_.GetAttestationKeyInfo(request, base::Bind(callback, &callback_count));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, ActivateAttestationKey) {
  auto fake_dbus_call =
      [](dbus::MethodCall* method_call,
         dbus::MockObjectProxy::ResponseCallback* response_callback) {
        // Verify request protobuf.
        dbus::MessageReader reader(method_call);
        ActivateAttestationKeyRequest request_proto;
        EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
        EXPECT_EQ("encrypted1",
                  request_proto.encrypted_certificate().asym_ca_contents());
        EXPECT_EQ("encrypted2",
                  request_proto.encrypted_certificate().sym_ca_attestation());
        EXPECT_TRUE(request_proto.save_certificate());
        // Create reply protobuf.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        ActivateAttestationKeyReply reply_proto;
        reply_proto.set_status(STATUS_SUCCESS);
        reply_proto.set_certificate("certificate");
        writer.AppendProtoAsArrayOfBytes(reply_proto);
        std::move(*response_callback).Run(response.get());
      };
  EXPECT_CALL(*mock_object_proxy_, DoCallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [](int* callback_count,
                     const ActivateAttestationKeyReply& reply) {
    (*callback_count)++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("certificate", reply.certificate());
  };
  ActivateAttestationKeyRequest request;
  request.mutable_encrypted_certificate()->set_asym_ca_contents("encrypted1");
  request.mutable_encrypted_certificate()->set_sym_ca_attestation("encrypted2");
  request.set_save_certificate(true);
  proxy_.ActivateAttestationKey(request, base::Bind(callback, &callback_count));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, CreateCertifiableKey) {
  auto fake_dbus_call =
      [](dbus::MethodCall* method_call,
         dbus::MockObjectProxy::ResponseCallback* response_callback) {
        // Verify request protobuf.
        dbus::MessageReader reader(method_call);
        CreateCertifiableKeyRequest request_proto;
        EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
        EXPECT_EQ("label", request_proto.key_label());
        EXPECT_EQ(KEY_TYPE_ECC, request_proto.key_type());
        EXPECT_EQ(KEY_USAGE_SIGN, request_proto.key_usage());
        EXPECT_EQ("user", request_proto.username());
        // Create reply protobuf.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        CreateCertifiableKeyReply reply_proto;
        reply_proto.set_status(STATUS_SUCCESS);
        reply_proto.set_public_key("public_key");
        reply_proto.set_certify_info("certify_info");
        reply_proto.set_certify_info_signature("signature");
        writer.AppendProtoAsArrayOfBytes(reply_proto);
        std::move(*response_callback).Run(response.get());
      };
  EXPECT_CALL(*mock_object_proxy_, DoCallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [](int* callback_count,
                     const CreateCertifiableKeyReply& reply) {
    (*callback_count)++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("public_key", reply.public_key());
    EXPECT_EQ("certify_info", reply.certify_info());
    EXPECT_EQ("signature", reply.certify_info_signature());
  };
  CreateCertifiableKeyRequest request;
  request.set_key_label("label");
  request.set_key_type(KEY_TYPE_ECC);
  request.set_key_usage(KEY_USAGE_SIGN);
  request.set_username("user");
  proxy_.CreateCertifiableKey(request, base::Bind(callback, &callback_count));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, Decrypt) {
  auto fake_dbus_call =
      [](dbus::MethodCall* method_call,
         dbus::MockObjectProxy::ResponseCallback* response_callback) {
        // Verify request protobuf.
        dbus::MessageReader reader(method_call);
        DecryptRequest request_proto;
        EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
        EXPECT_EQ("label", request_proto.key_label());
        EXPECT_EQ("user", request_proto.username());
        EXPECT_EQ("data", request_proto.encrypted_data());
        // Create reply protobuf.
        std::unique_ptr<dbus::Response> response =
            dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        DecryptReply reply_proto;
        reply_proto.set_status(STATUS_SUCCESS);
        reply_proto.set_decrypted_data("data");
        writer.AppendProtoAsArrayOfBytes(reply_proto);
        std::move(*response_callback).Run(response.get());
      };
  EXPECT_CALL(*mock_object_proxy_, DoCallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [](int* callback_count, const DecryptReply& reply) {
    (*callback_count)++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("data", reply.decrypted_data());
  };
  DecryptRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_encrypted_data("data");
  proxy_.Decrypt(request, base::Bind(callback, &callback_count));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, Sign) {
  auto fake_dbus_call =
      [](dbus::MethodCall* method_call,
         dbus::MockObjectProxy::ResponseCallback* response_callback) {
        // Verify request protobuf.
        dbus::MessageReader reader(method_call);
        SignRequest request_proto;
        EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
        EXPECT_EQ("label", request_proto.key_label());
        EXPECT_EQ("user", request_proto.username());
        EXPECT_EQ("data", request_proto.data_to_sign());
        // Create reply protobuf.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        SignReply reply_proto;
        reply_proto.set_status(STATUS_SUCCESS);
        reply_proto.set_signature("signature");
        writer.AppendProtoAsArrayOfBytes(reply_proto);
        std::move(*response_callback).Run(response.get());
      };
  EXPECT_CALL(*mock_object_proxy_, DoCallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [](int* callback_count, const SignReply& reply) {
    (*callback_count)++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
    EXPECT_EQ("signature", reply.signature());
  };
  SignRequest request;
  request.set_key_label("label");
  request.set_username("user");
  request.set_data_to_sign("data");
  proxy_.Sign(request, base::Bind(callback, &callback_count));
  EXPECT_EQ(1, callback_count);
}

TEST_F(DBusProxyTest, RegisterKeyWithChapsToken) {
  auto fake_dbus_call =
      [](dbus::MethodCall* method_call,
         dbus::MockObjectProxy::ResponseCallback* response_callback) {
        // Verify request protobuf.
        dbus::MessageReader reader(method_call);
        RegisterKeyWithChapsTokenRequest request_proto;
        EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&request_proto));
        EXPECT_EQ("label", request_proto.key_label());
        EXPECT_EQ("user", request_proto.username());
        // Create reply protobuf.
        auto response = dbus::Response::CreateEmpty();
        dbus::MessageWriter writer(response.get());
        RegisterKeyWithChapsTokenReply reply_proto;
        reply_proto.set_status(STATUS_SUCCESS);
        writer.AppendProtoAsArrayOfBytes(reply_proto);
        std::move(*response_callback).Run(response.get());
      };
  EXPECT_CALL(*mock_object_proxy_, DoCallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(WithArgs<0, 2>(Invoke(fake_dbus_call)));

  // Set expectations on the outputs.
  int callback_count = 0;
  auto callback = [](int* callback_count,
                     const RegisterKeyWithChapsTokenReply& reply) {
    (*callback_count)++;
    EXPECT_EQ(STATUS_SUCCESS, reply.status());
  };
  RegisterKeyWithChapsTokenRequest request;
  request.set_key_label("label");
  request.set_username("user");
  proxy_.RegisterKeyWithChapsToken(request,
                                   base::Bind(callback, &callback_count));
  EXPECT_EQ(1, callback_count);
}

}  // namespace attestation
