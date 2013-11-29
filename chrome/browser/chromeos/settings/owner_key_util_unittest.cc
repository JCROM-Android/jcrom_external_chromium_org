// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/owner_key_util.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "crypto/rsa_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

// 2048-bit RSA public key for testing.
const uint8 kTestKeyData[] = {
    0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
    0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xe8, 0x39, 0x11,
    0xd0, 0x98, 0x52, 0x4f, 0xf7, 0x18, 0xd1, 0xbf, 0x98, 0x06, 0xae, 0x7a,
    0x7c, 0xd7, 0x6f, 0x02, 0x54, 0x37, 0x4e, 0xcd, 0xa6, 0x27, 0x8e, 0xf7,
    0x82, 0x1d, 0xde, 0x3d, 0xf5, 0x6b, 0xa4, 0xe5, 0x6b, 0x0c, 0xf0, 0x39,
    0xe5, 0xd9, 0x53, 0xe7, 0x6b, 0x6d, 0xa1, 0xc7, 0xdf, 0x92, 0xb7, 0xb0,
    0x14, 0x4d, 0x4e, 0x7d, 0x27, 0xd4, 0xf7, 0x35, 0xae, 0xc7, 0x3d, 0x13,
    0x74, 0x23, 0x8c, 0xda, 0xf1, 0x94, 0xbb, 0x2e, 0x06, 0x0d, 0x55, 0xe2,
    0x29, 0xf1, 0xfb, 0x4a, 0x2a, 0xb2, 0x40, 0x62, 0x59, 0x21, 0x39, 0xf9,
    0xd2, 0x1e, 0x12, 0xe1, 0x33, 0xab, 0x7e, 0xa9, 0x23, 0x06, 0x1b, 0x03,
    0x90, 0xbd, 0x60, 0x30, 0x0c, 0xda, 0x7f, 0x99, 0x6c, 0xd7, 0xd4, 0xe9,
    0xc9, 0xe6, 0xea, 0x7b, 0x47, 0x4c, 0x53, 0xe1, 0xe9, 0x62, 0xe4, 0xa4,
    0x6c, 0xbc, 0xa2, 0xe0, 0xbe, 0xf3, 0xe5, 0x5e, 0x19, 0xe0, 0x94, 0xd3,
    0x01, 0x97, 0x2a, 0x8d, 0x21, 0x2f, 0xa7, 0xc5, 0x74, 0xa9, 0xd0, 0x23,
    0x9e, 0x9a, 0x87, 0x68, 0xe5, 0x96, 0x51, 0xb1, 0xe2, 0x06, 0xa5, 0xe0,
    0xaa, 0x73, 0xf3, 0xeb, 0xaf, 0x8c, 0xaf, 0xb2, 0x34, 0x8a, 0x44, 0xec,
    0x5e, 0x84, 0x5f, 0x8c, 0xa4, 0x90, 0xf7, 0x89, 0xf4, 0xc1, 0x73, 0x93,
    0x08, 0x7e, 0x1a, 0x16, 0x65, 0x44, 0xff, 0x2d, 0x4e, 0x62, 0xbf, 0x32,
    0x81, 0xec, 0xcf, 0xc1, 0xac, 0x3e, 0x0b, 0xd4, 0xc1, 0xe1, 0x7d, 0x15,
    0x09, 0xd7, 0xd8, 0xe8, 0xba, 0x3e, 0x76, 0xb2, 0x3f, 0x1d, 0x31, 0x5b,
    0x20, 0x6e, 0xe1, 0x73, 0xc5, 0x58, 0x46, 0xf5, 0x24, 0xdc, 0xe7, 0x95,
    0xb4, 0xf0, 0x27, 0x4b, 0x15, 0x18, 0x5c, 0x95, 0xfe, 0x21, 0x04, 0x68,
    0xe7, 0xde, 0x16, 0x66, 0x60, 0xd7, 0xda, 0xc5, 0x5b, 0xda, 0x0f, 0xfa,
    0x41, 0x02, 0x03, 0x01, 0x00, 0x01,
};

class OwnerKeyUtilTest : public testing::Test {
 protected:
  OwnerKeyUtilTest() {}
  virtual ~OwnerKeyUtilTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(tmpdir_.CreateUniqueTempDir());
    key_file_ = tmpdir_.path().Append("key");
    util_ = new OwnerKeyUtilImpl(key_file_);
  }

  base::ScopedTempDir tmpdir_;
  base::FilePath key_file_;
  scoped_refptr<OwnerKeyUtil> util_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OwnerKeyUtilTest);
};

TEST_F(OwnerKeyUtilTest, ImportPublicKey) {
  // Export public key, so that we can compare it to the one we get off disk.
  std::vector<uint8> public_key(kTestKeyData,
                                kTestKeyData + sizeof(kTestKeyData));
  ASSERT_EQ(static_cast<int>(public_key.size()),
            file_util::WriteFile(
                key_file_,
                reinterpret_cast<const char*>(vector_as_array(&public_key)),
                public_key.size()));
  EXPECT_TRUE(util_->IsPublicKeyPresent());

  std::vector<uint8> from_disk;
  EXPECT_TRUE(util_->ImportPublicKey(&from_disk));

  EXPECT_EQ(public_key, from_disk);
}

TEST_F(OwnerKeyUtilTest, ImportPublicKeyFailed) {
  // First test the case where the file is missing which should fail.
  EXPECT_FALSE(util_->IsPublicKeyPresent());
  std::vector<uint8> from_disk;
  EXPECT_FALSE(util_->ImportPublicKey(&from_disk));

  // Next try empty file. This should fail and the array should be empty.
  from_disk.resize(10);
  ASSERT_EQ(0, file_util::WriteFile(key_file_, "", 0));
  EXPECT_TRUE(util_->IsPublicKeyPresent());
  EXPECT_FALSE(util_->ImportPublicKey(&from_disk));
  EXPECT_FALSE(from_disk.size());
}

}  // namespace chromeos
