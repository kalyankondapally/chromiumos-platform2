// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MAIN_TEST_H_
#define MAIN_TEST_H_

#ifndef IN_MOBILE_OPERATOR_INFO_UNITTEST_CC
  #error "Must be included only from mobile_operator_info_unittest.cc."
#endif

// Following is the binary protobuf for the following (text representation)
// protobuf:
// # Each MNO below has a unique three digit ID, specified before the MNO.
// # You should use this ID to generate unique fields where needed.
// # Specificially, in the mccmnc, name, uuid fields.
//
// # Test[101]: MNOByMCCMNC
// mno {
//   data {
//     mccmnc: "101001"
//     localized_name {
//       name: "name101"
//     }
//     uuid: "uuid101"
//   }
// }
//
// # Test[102]: MNOByMCCMNCMultipleOptions
// mno {
//   data {
//     mccmnc: "102001"
//     mccmnc: "102002"
//     localized_name {
//       name: "name102"
//     }
//     uuid: "uuid102"
//   }
// }
//
// # Test[103]: MNOByOperatorName
// mno {
//   data {
//     mccmnc: "103001"
//     localized_name {
//       name: "name103"
//     }
//     uuid: "uuid103"
//   }
// }
//
// # Test[104]: MNOByOperatorNameMultipleOptions
// mno {
//   data {
//     mccmnc: "104001"
//     localized_name {
//       name: "name104001"
//     }
//     localized_name {
//       name: "name104002"
//     }
//     uuid: "uuid104"
//   }
// }
//
// # Tets[105]: MNOByOperatorNameWithLang
// mno {
//   data {
//     mccmnc: "105001"
//     localized_name {
//       name: "name105"
//       language: "en"
//     }
//     uuid: "uuid105"
//   }
// }
//
// # Test[106]: MNOByMCCMNCAndOperatorName
// mno {
//   data {
//     mccmnc: "106001"
//     localized_name {
//       name: "name106001"
//     }
//     uuid: "uuid106001"
//   }
// }
// mno {
//   data {
//     mccmnc: "106001"
//     localized_name {
//       name: "name106002"
//     }
//   uuid: "uuid106002"
//   }
// }
//
// # Test[107]: MNOByOperatorNameAndMCCMNC
// mno {
//   data {
//     mccmnc: "107001"
//     localized_name {
//       name: "name107"
//     }
//     uuid: "uuid107001"
//   }
// }
// mno {
//   data {
//     mccmnc: "107002"
//     localized_name {
//       name: "name107"
//     }
//   uuid: "uuid107002"
//   }
// }
//
// # Test[108]: MNOByMCCMNCOberridesOperatorName
// mno {
//   data {
//     mccmnc: "108001"
//     localized_name {
//       name: "name108001"
//     }
//     uuid: "uuid108001"
//   }
// }
// mno {
//   data {
//     mccmnc: "108002"
//     localized_name {
//       name: "name108002"
//     }
//     uuid: "uuid108002"
//   }
// }
//
// # Test[109]: MNOByIMSI
// mno {
//   data {
//     mccmnc: "10901"
//     localized_name {
//       name: "name10901"
//     }
//     uuid: "uuid10901"
//   }
// }
// mno {
//   data {
//     mccmnc: "109002"
//     localized_name {
//       name: "name109002"
//     }
//     uuid: "uuid109002"
//   }
// }
//
// # Test[110]: MNOByMCCMNCOverridesIMSI
// mno {
//   data {
//     mccmnc: "110001"
//     localized_name {
//       name: "name110001"
//     }
//     uuid: "uuid110001"
//   }
// }
// mno {
//   data {
//     mccmnc: "110002"
//     localized_name {
//       name: "name110002"
//     }
//     uuid: "uuid110002"
//   }
// }
//
// # Test[111] MNOUnchangedBySecondaryUpdates
// mno {
//   data {
//     mccmnc: "111001"
//     localized_name {
//       name: "name111001"
//     }
//     uuid: "uuid111001"
//   }
// }
// mno {
//   data {
//     mccmnc: "111002"
//     sid: "111102"
//     nid: "111202"
//     uuid: "uuid111002"
//   }
// }
//
// # Test[112] MVNODefaultMatch
// mno {
//   data {
//     mccmnc: "112001"
//     localized_name {
//       name: "name112001"
//     }
//     uuid: "uuid112001"
//   }
//   mvno {
//     data {
//       uuid: "uuid112002"
//     }
//   }
// }
//
// # Test[113] MVNONameMatch & MVNOMatchAndMismatch & MVNOMatchAndReset
// mno {
//   data {
//     mccmnc: "113001"
//     localized_name {
//       name: "name113001"
//     }
//     uuid: "uuid113001"
//   }
//   mvno {
//     mvno_filter {
//       type: OPERATOR_NAME
//       regex: "name113002"
//     }
//     data {
//       localized_name {
//         name: "name113002"
//       }
//       uuid: "uuid113002"
//     }
//   }
// }
//
// # Test[114] MVNONameMalformedRegexMatch
// mno {
//   data {
//     mccmnc: "114001"
//     localized_name {
//       name: "name114001"
//     }
//     uuid: "uuid114001"
//   }
//   # All mvnos have malformed filters.
//   mvno {
//     mvno_filter {
//       type: OPERATOR_NAME
//       regex: "name["
//     }
//     data {
//       localized_name {
//         name: "name114002"
//       }
//       uuid: "name114002"
//     }
//   }
// }
//
// # Test[115] MVNONameSubexpressionRegexMatch
// mno {
//   data {
//     mccmnc: "115001"
//     localized_name {
//       name: "name115001"
//     }
//     uuid: "uuid115001"
//   }
//   mvno {
//     mvno_filter {
//       type: OPERATOR_NAME
//       regex: "name115"
//     }
//     data {
//       localized_name {
//         name: "name115002"
//       }
//       uuid: "uuid115002"
//     }
//   }
// }
//
// # Test[116] MVNONameSubexpressionRegexMatch
// mno {
//   data {
//     mccmnc: "116001"
//     localized_name {
//       name: "name116001"
//     }
//     uuid: "uuid116001"
//   }
//   mvno {
//     mvno_filter {
//       type: OPERATOR_NAME
//       regex: "name[a-zA-Z]*116[0-9]{0,3}"
//     }
//     data {
//       localized_name {
//         name: "name116002"
//       }
//       uuid: "uuid116002"
//     }
//   }
// }
//
// # Test[117] MVNONameMatchMultipleFilters
// mno {
//   data {
//     mccmnc: "117001"
//     localized_name {
//       name: "name117001"
//     }
//     uuid: "uuid117001"
//   }
//   mvno {
//     mvno_filter {
//       type: OPERATOR_NAME
//       regex: "nameA_[a-zA-Z]*"
//     }
//     mvno_filter {
//       type: OPERATOR_NAME
//       regex: "[a-zA-Z]*_nameB"
//     }
//     data {
//       localized_name {
//         name: "name117002"
//       }
//       uuid: "uuid117002"
//     }
//   }
// }
//
// # Test[118] MVNOIMSIMatch
// mno {
//   data {
//     mccmnc: "118001"
//     localized_name {
//       name: "name118001"
//     }
//     uuid: "uuid118001"
//   }
//   mvno {
//     mvno_filter {
//       type: IMSI
//       regex: "1180015432154321"
//     }
//     data {
//       localized_name {
//         name: "name118002"
//       }
//       uuid: "uuid118002"
//     }
//   }
// }
//
// # Test[119] MVNOICCIDMatch
// mno {
//   data {
//     mccmnc: "119001"
//     localized_name {
//       name: "name119001"
//     }
//     uuid: "uuid119001"
//   }
//   mvno {
//     mvno_filter {
//       type: ICCID
//       regex: "119123456789"
//     }
//     data {
//       localized_name {
//         name: "name119002"
//       }
//       uuid: "uuid119002"
//     }
//   }
// }
//
// # Test[120] MVNOSIDMatch
// mno {
//   data {
//     sid: "120001"
//     sid: "120002"
//     localized_name {
//       name: "name120001"
//     }
//     uuid: "uuid120001"
//   }
//   mvno {
//     mvno_filter {
//       type: SID
//       regex: "120002"
//     }
//     data {
//       localized_name {
//         name: "name120002"
//       }
//       uuid: "uuid120002"
//     }
//   }
// }
//
// # Test[121] MVNOAllMatch
// mno {
//   data {
//     mccmnc: "121001"
//     localized_name {
//       name: "name121001"
//     }
//     uuid: "uuid121001"
//   }
//   mvno {
//     mvno_filter {
//       type: OPERATOR_NAME
//       regex: "name121003"
//     }
//     data {
//       localized_name {
//         name: "name121003"
//       }
//       uuid: "uuid121003"
//     }
//   }
//   mvno {
//     mvno_filter {
//       type: IMSI
//       regex: "1210045432154321"
//     }
//     data {
//       localized_name {
//         name: "name121004"
//       }
//       uuid: "uuid121004"
//     }
//   }
//   mvno {
//     mvno_filter {
//       type: ICCID
//       regex: "121005123456789"
//     }
//     data {
//       localized_name {
//         name: "name121005"
//       }
//       uuid: "uuid121005"
//     }
//   }
//   mvno {
//     mvno_filter {
//       type: OPERATOR_NAME
//       regex: "name121006"
//     }
//     mvno_filter {
//       type: ICCID
//       regex: "121006123456789"
//     }
//     data {
//       localized_name {
//         name: "name121006"
//       }
//       uuid: "uuid121006"
//     }
//   }
// }
//
// # Test[122]: MNOBySID
// mno {
//   data {
//     sid: "1221"
//     localized_name {
//       name: "name1221"
//     }
//     uuid: "uuid1221"
//   }
// }
//
// # Test[123] MNOByMCCMNCAndSID
// mno {
//   data {
//     mccmnc: "123001"
//     localized_name {
//       name: "name123001"
//     }
//     uuid: "uuid123001"
//   }
// }
// mno {
//   data {
//     sid: "1232"
//     localized_name {
//       name: "name1232"
//     }
//     uuid: "uuid1232"
//   }
// }
//
// The binary data for the protobuf in this file was generated by writing the
// prototxt file main_test.prototxt and then:
//   protoc --proto_path .. --encode "shill.mobile_operator_db.MobileOperatorDB"
//     ../mobile_operator_db.proto < main_test.prototxt
//     > main_test.h.pbf
//   cat main_test.h.pbf | xxd -i

namespace shill {
namespace mobile_operator_db {
static const unsigned char main_test[] {
  0x0a, 0x1f, 0x0a, 0x1d, 0x0a, 0x07, 0x75, 0x75, 0x69, 0x64, 0x31, 0x30,
  0x31, 0x22, 0x09, 0x0a, 0x07, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x30, 0x31,
  0xaa, 0x01, 0x06, 0x31, 0x30, 0x31, 0x30, 0x30, 0x31, 0x0a, 0x28, 0x0a,
  0x26, 0x0a, 0x07, 0x75, 0x75, 0x69, 0x64, 0x31, 0x30, 0x32, 0x22, 0x09,
  0x0a, 0x07, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x30, 0x32, 0xaa, 0x01, 0x06,
  0x31, 0x30, 0x32, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06, 0x31, 0x30, 0x32,
  0x30, 0x30, 0x32, 0x0a, 0x1f, 0x0a, 0x1d, 0x0a, 0x07, 0x75, 0x75, 0x69,
  0x64, 0x31, 0x30, 0x33, 0x22, 0x09, 0x0a, 0x07, 0x6e, 0x61, 0x6d, 0x65,
  0x31, 0x30, 0x33, 0xaa, 0x01, 0x06, 0x31, 0x30, 0x33, 0x30, 0x30, 0x31,
  0x0a, 0x30, 0x0a, 0x2e, 0x0a, 0x07, 0x75, 0x75, 0x69, 0x64, 0x31, 0x30,
  0x34, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x30, 0x34,
  0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31,
  0x30, 0x34, 0x30, 0x30, 0x32, 0xaa, 0x01, 0x06, 0x31, 0x30, 0x34, 0x30,
  0x30, 0x31, 0x0a, 0x23, 0x0a, 0x21, 0x0a, 0x07, 0x75, 0x75, 0x69, 0x64,
  0x31, 0x30, 0x35, 0x22, 0x0d, 0x0a, 0x07, 0x6e, 0x61, 0x6d, 0x65, 0x31,
  0x30, 0x35, 0x12, 0x02, 0x65, 0x6e, 0xaa, 0x01, 0x06, 0x31, 0x30, 0x35,
  0x30, 0x30, 0x31, 0x0a, 0x25, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69,
  0x64, 0x31, 0x30, 0x36, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e,
  0x61, 0x6d, 0x65, 0x31, 0x30, 0x36, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06,
  0x31, 0x30, 0x36, 0x30, 0x30, 0x31, 0x0a, 0x25, 0x0a, 0x23, 0x0a, 0x0a,
  0x75, 0x75, 0x69, 0x64, 0x31, 0x30, 0x36, 0x30, 0x30, 0x32, 0x22, 0x0c,
  0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x30, 0x36, 0x30, 0x30, 0x32,
  0xaa, 0x01, 0x06, 0x31, 0x30, 0x36, 0x30, 0x30, 0x31, 0x0a, 0x22, 0x0a,
  0x20, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x30, 0x37, 0x30, 0x30,
  0x31, 0x22, 0x09, 0x0a, 0x07, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x30, 0x37,
  0xaa, 0x01, 0x06, 0x31, 0x30, 0x37, 0x30, 0x30, 0x31, 0x0a, 0x22, 0x0a,
  0x20, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x30, 0x37, 0x30, 0x30,
  0x32, 0x22, 0x09, 0x0a, 0x07, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x30, 0x37,
  0xaa, 0x01, 0x06, 0x31, 0x30, 0x37, 0x30, 0x30, 0x32, 0x0a, 0x25, 0x0a,
  0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x30, 0x38, 0x30, 0x30,
  0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x30, 0x38,
  0x30, 0x30, 0x31, 0xaa, 0x01, 0x06, 0x31, 0x30, 0x38, 0x30, 0x30, 0x31,
  0x0a, 0x25, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x30,
  0x38, 0x30, 0x30, 0x32, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65,
  0x31, 0x30, 0x38, 0x30, 0x30, 0x32, 0xaa, 0x01, 0x06, 0x31, 0x30, 0x38,
  0x30, 0x30, 0x32, 0x0a, 0x22, 0x0a, 0x20, 0x0a, 0x09, 0x75, 0x75, 0x69,
  0x64, 0x31, 0x30, 0x39, 0x30, 0x31, 0x22, 0x0b, 0x0a, 0x09, 0x6e, 0x61,
  0x6d, 0x65, 0x31, 0x30, 0x39, 0x30, 0x31, 0xaa, 0x01, 0x05, 0x31, 0x30,
  0x39, 0x30, 0x31, 0x0a, 0x25, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69,
  0x64, 0x31, 0x30, 0x39, 0x30, 0x30, 0x32, 0x22, 0x0c, 0x0a, 0x0a, 0x6e,
  0x61, 0x6d, 0x65, 0x31, 0x30, 0x39, 0x30, 0x30, 0x32, 0xaa, 0x01, 0x06,
  0x31, 0x30, 0x39, 0x30, 0x30, 0x32, 0x0a, 0x25, 0x0a, 0x23, 0x0a, 0x0a,
  0x75, 0x75, 0x69, 0x64, 0x31, 0x31, 0x30, 0x30, 0x30, 0x31, 0x22, 0x0c,
  0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x30, 0x30, 0x30, 0x31,
  0xaa, 0x01, 0x06, 0x31, 0x31, 0x30, 0x30, 0x30, 0x31, 0x0a, 0x25, 0x0a,
  0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31, 0x30, 0x30, 0x30,
  0x32, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x30,
  0x30, 0x30, 0x32, 0xaa, 0x01, 0x06, 0x31, 0x31, 0x30, 0x30, 0x30, 0x32,
  0x0a, 0x25, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31,
  0x31, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65,
  0x31, 0x31, 0x31, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06, 0x31, 0x31, 0x31,
  0x30, 0x30, 0x31, 0x0a, 0x29, 0x0a, 0x27, 0x0a, 0x0a, 0x75, 0x75, 0x69,
  0x64, 0x31, 0x31, 0x31, 0x30, 0x30, 0x32, 0xaa, 0x01, 0x06, 0x31, 0x31,
  0x31, 0x30, 0x30, 0x32, 0xca, 0x02, 0x06, 0x31, 0x31, 0x31, 0x31, 0x30,
  0x32, 0xd2, 0x02, 0x06, 0x31, 0x31, 0x31, 0x32, 0x30, 0x32, 0x0a, 0x35,
  0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31, 0x32, 0x30,
  0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31,
  0x32, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06, 0x31, 0x31, 0x32, 0x30, 0x30,
  0x31, 0x12, 0x0e, 0x12, 0x0c, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31,
  0x31, 0x32, 0x30, 0x30, 0x32, 0x0a, 0x53, 0x0a, 0x23, 0x0a, 0x0a, 0x75,
  0x75, 0x69, 0x64, 0x31, 0x31, 0x33, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a,
  0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x33, 0x30, 0x30, 0x31, 0xaa,
  0x01, 0x06, 0x31, 0x31, 0x33, 0x30, 0x30, 0x31, 0x12, 0x2c, 0x0a, 0x0e,
  0x08, 0x04, 0x12, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x33, 0x30,
  0x30, 0x32, 0x12, 0x1a, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31,
  0x33, 0x30, 0x30, 0x32, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65,
  0x31, 0x31, 0x33, 0x30, 0x30, 0x32, 0x0a, 0x4e, 0x0a, 0x23, 0x0a, 0x0a,
  0x75, 0x75, 0x69, 0x64, 0x31, 0x31, 0x34, 0x30, 0x30, 0x31, 0x22, 0x0c,
  0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x34, 0x30, 0x30, 0x31,
  0xaa, 0x01, 0x06, 0x31, 0x31, 0x34, 0x30, 0x30, 0x31, 0x12, 0x27, 0x0a,
  0x09, 0x08, 0x04, 0x12, 0x05, 0x6e, 0x61, 0x6d, 0x65, 0x5b, 0x12, 0x1a,
  0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x34, 0x30, 0x30, 0x32,
  0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x34, 0x30,
  0x30, 0x32, 0x0a, 0x50, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64,
  0x31, 0x31, 0x35, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61,
  0x6d, 0x65, 0x31, 0x31, 0x35, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06, 0x31,
  0x31, 0x35, 0x30, 0x30, 0x31, 0x12, 0x29, 0x0a, 0x0b, 0x08, 0x04, 0x12,
  0x07, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x35, 0x12, 0x1a, 0x0a, 0x0a,
  0x75, 0x75, 0x69, 0x64, 0x31, 0x31, 0x35, 0x30, 0x30, 0x32, 0x22, 0x0c,
  0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x35, 0x30, 0x30, 0x32,
  0x0a, 0x63, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31,
  0x36, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65,
  0x31, 0x31, 0x36, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06, 0x31, 0x31, 0x36,
  0x30, 0x30, 0x31, 0x12, 0x3c, 0x0a, 0x1e, 0x08, 0x04, 0x12, 0x1a, 0x6e,
  0x61, 0x6d, 0x65, 0x5b, 0x61, 0x2d, 0x7a, 0x41, 0x2d, 0x5a, 0x5d, 0x2a,
  0x31, 0x31, 0x36, 0x5b, 0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x30, 0x2c, 0x33,
  0x7d, 0x12, 0x1a, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31, 0x36,
  0x30, 0x30, 0x32, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31,
  0x31, 0x36, 0x30, 0x30, 0x32, 0x0a, 0x6d, 0x0a, 0x23, 0x0a, 0x0a, 0x75,
  0x75, 0x69, 0x64, 0x31, 0x31, 0x37, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a,
  0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x37, 0x30, 0x30, 0x31, 0xaa,
  0x01, 0x06, 0x31, 0x31, 0x37, 0x30, 0x30, 0x31, 0x12, 0x46, 0x0a, 0x13,
  0x08, 0x04, 0x12, 0x0f, 0x6e, 0x61, 0x6d, 0x65, 0x41, 0x5f, 0x5b, 0x61,
  0x2d, 0x7a, 0x41, 0x2d, 0x5a, 0x5d, 0x2a, 0x0a, 0x13, 0x08, 0x04, 0x12,
  0x0f, 0x5b, 0x61, 0x2d, 0x7a, 0x41, 0x2d, 0x5a, 0x5d, 0x2a, 0x5f, 0x6e,
  0x61, 0x6d, 0x65, 0x42, 0x12, 0x1a, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64,
  0x31, 0x31, 0x37, 0x30, 0x30, 0x32, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61,
  0x6d, 0x65, 0x31, 0x31, 0x37, 0x30, 0x30, 0x32, 0x0a, 0x59, 0x0a, 0x23,
  0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31, 0x38, 0x30, 0x30, 0x31,
  0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x38, 0x30,
  0x30, 0x31, 0xaa, 0x01, 0x06, 0x31, 0x31, 0x38, 0x30, 0x30, 0x31, 0x12,
  0x32, 0x0a, 0x14, 0x08, 0x01, 0x12, 0x10, 0x31, 0x31, 0x38, 0x30, 0x30,
  0x31, 0x35, 0x34, 0x33, 0x32, 0x31, 0x35, 0x34, 0x33, 0x32, 0x31, 0x12,
  0x1a, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31, 0x38, 0x30, 0x30,
  0x32, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x31, 0x38,
  0x30, 0x30, 0x32, 0x0a, 0x55, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69,
  0x64, 0x31, 0x31, 0x39, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e,
  0x61, 0x6d, 0x65, 0x31, 0x31, 0x39, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06,
  0x31, 0x31, 0x39, 0x30, 0x30, 0x31, 0x12, 0x2e, 0x0a, 0x10, 0x08, 0x02,
  0x12, 0x0c, 0x31, 0x31, 0x39, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x12, 0x1a, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x31,
  0x39, 0x30, 0x30, 0x32, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65,
  0x31, 0x31, 0x39, 0x30, 0x30, 0x32, 0x0a, 0x58, 0x0a, 0x2c, 0x0a, 0x0a,
  0x75, 0x75, 0x69, 0x64, 0x31, 0x32, 0x30, 0x30, 0x30, 0x31, 0x22, 0x0c,
  0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x32, 0x30, 0x30, 0x30, 0x31,
  0xca, 0x02, 0x06, 0x31, 0x32, 0x30, 0x30, 0x30, 0x31, 0xca, 0x02, 0x06,
  0x31, 0x32, 0x30, 0x30, 0x30, 0x32, 0x12, 0x28, 0x0a, 0x0a, 0x08, 0x03,
  0x12, 0x06, 0x31, 0x32, 0x30, 0x30, 0x30, 0x32, 0x12, 0x1a, 0x0a, 0x0a,
  0x75, 0x75, 0x69, 0x64, 0x31, 0x32, 0x30, 0x30, 0x30, 0x32, 0x22, 0x0c,
  0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x32, 0x30, 0x30, 0x30, 0x32,
  0x0a, 0xfd, 0x01, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31,
  0x32, 0x31, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d,
  0x65, 0x31, 0x32, 0x31, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06, 0x31, 0x32,
  0x31, 0x30, 0x30, 0x31, 0x12, 0x2c, 0x0a, 0x0e, 0x08, 0x04, 0x12, 0x0a,
  0x6e, 0x61, 0x6d, 0x65, 0x31, 0x32, 0x31, 0x30, 0x30, 0x33, 0x12, 0x1a,
  0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x32, 0x31, 0x30, 0x30, 0x33,
  0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x32, 0x31, 0x30,
  0x30, 0x33, 0x12, 0x32, 0x0a, 0x14, 0x08, 0x01, 0x12, 0x10, 0x31, 0x32,
  0x31, 0x30, 0x30, 0x34, 0x35, 0x34, 0x33, 0x32, 0x31, 0x35, 0x34, 0x33,
  0x32, 0x31, 0x12, 0x1a, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x32,
  0x31, 0x30, 0x30, 0x34, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65,
  0x31, 0x32, 0x31, 0x30, 0x30, 0x34, 0x12, 0x31, 0x0a, 0x13, 0x08, 0x02,
  0x12, 0x0f, 0x31, 0x32, 0x31, 0x30, 0x30, 0x35, 0x31, 0x32, 0x33, 0x34,
  0x35, 0x36, 0x37, 0x38, 0x39, 0x12, 0x1a, 0x0a, 0x0a, 0x75, 0x75, 0x69,
  0x64, 0x31, 0x32, 0x31, 0x30, 0x30, 0x35, 0x22, 0x0c, 0x0a, 0x0a, 0x6e,
  0x61, 0x6d, 0x65, 0x31, 0x32, 0x31, 0x30, 0x30, 0x35, 0x12, 0x41, 0x0a,
  0x0e, 0x08, 0x04, 0x12, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x32, 0x31,
  0x30, 0x30, 0x36, 0x0a, 0x13, 0x08, 0x02, 0x12, 0x0f, 0x31, 0x32, 0x31,
  0x30, 0x30, 0x36, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x12, 0x1a, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31, 0x32, 0x31, 0x30,
  0x30, 0x36, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d, 0x65, 0x31, 0x32,
  0x31, 0x30, 0x30, 0x36, 0x0a, 0x1f, 0x0a, 0x1d, 0x0a, 0x08, 0x75, 0x75,
  0x69, 0x64, 0x31, 0x32, 0x32, 0x31, 0x22, 0x0a, 0x0a, 0x08, 0x6e, 0x61,
  0x6d, 0x65, 0x31, 0x32, 0x32, 0x31, 0xca, 0x02, 0x04, 0x31, 0x32, 0x32,
  0x31, 0x0a, 0x25, 0x0a, 0x23, 0x0a, 0x0a, 0x75, 0x75, 0x69, 0x64, 0x31,
  0x32, 0x33, 0x30, 0x30, 0x31, 0x22, 0x0c, 0x0a, 0x0a, 0x6e, 0x61, 0x6d,
  0x65, 0x31, 0x32, 0x33, 0x30, 0x30, 0x31, 0xaa, 0x01, 0x06, 0x31, 0x32,
  0x33, 0x30, 0x30, 0x31, 0x0a, 0x1f, 0x0a, 0x1d, 0x0a, 0x08, 0x75, 0x75,
  0x69, 0x64, 0x31, 0x32, 0x33, 0x32, 0x22, 0x0a, 0x0a, 0x08, 0x6e, 0x61,
  0x6d, 0x65, 0x31, 0x32, 0x33, 0x32, 0xca, 0x02, 0x04, 0x31, 0x32, 0x33,
  0x32
};
}  // namespace mobile_operator_db
}  // namespace shill

#endif  // MAIN_TEST_H_
