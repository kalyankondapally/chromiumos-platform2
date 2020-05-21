// Copyright 2020 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HWSEC_TEST_UTILS_FAKE_PCA_TPM1_STRUCT_UTILS_H_
#define HWSEC_TEST_UTILS_FAKE_PCA_TPM1_STRUCT_UTILS_H_

#if USE_TPM2
#error "This file is used for TPM1.2 only"
#endif

#include <string>

#include <base/optional.h>
#include <crypto/scoped_openssl_types.h>
#include <trousers/tss.h>

namespace hwsec_test_utils {
namespace fake_pca_agent {

// Parse |serialized_tpm_pubkey| into |TPM_PUBKEY| and return the public key in
// |crypto::ScopedEVP_PKEY|. If |public_key_digest|, it is set to the SHA-1
// digest of the public key.
crypto::ScopedEVP_PKEY TpmPublicKeyToEVP(
    const std::string& serialized_tpm_pubkey,
    std::string* public_key_digest);

// Builds the serialized TPM_PCR_COMPOSITE stream with |pcr_value| at
// |pcr_index|.
std::string ToPcrComposite(uint32_t pcr_index, const std::string& pcr_value);

// Serialize |contents| of |TPM_ASYM_CA_CONTENTS| type.
std::string Serialize(TPM_ASYM_CA_CONTENTS* contents);

// Serialize |contents| of |TPM_SYM_CA_ATTESTATION| type.
std::string Serialize(TPM_SYM_CA_ATTESTATION* contents);

}  // namespace fake_pca_agent
}  // namespace hwsec_test_utils

#endif  // HWSEC_TEST_UTILS_FAKE_PCA_TPM1_STRUCT_UTILS_H_
