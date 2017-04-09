// sshfp_record_test.cc - Tests for sshfp_record.{h,cc}.

// Copyright 2016, 2017 Richard Woodbury
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "mosh_nacl/sshfp_record.h"

#include <assert.h>

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "mosh_nacl/ssh.h"

using std::string;
using std::vector;

class SSHFPRecordSetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rsa_key_.ImportPublicKey(
        "AAAAB3NzaC1yc2EAAAADAQABAAABAQCgOJH7TgVaUtYMFkqJjjojUZYjq2spRihSx9U0MJ"
        "4pUMLnkV+MzuXWYN89TDkBrdw+xeYvL9KVs624sFJWa2KdGGC29uzZhHh6UC7sUy7CdXuu"
        "oNoukxnHjBuU74SkMLW4MagerN4eFq4l8F15anjzpmQ9/CjzqXKwMeITbxgzsGdDtyMswR"
        "X+KGk0leY+nmsw1E56RQoRgwIXJ6mLuep/WL3IBxoePJ+zZcremExWpxXjre3+F+aXRoRC"
        "ASKHnd4nol7AlP4GiKJLPYWbVHJ5bzHo1WO5P2PVJvUQ13O8TMaYEmXs6RMq40RHKFEtMT"
        "Rw39IInT7Ck63nmB3n5o8n",
        ssh::KeyType(ssh::KeyType::RSA)));
    ASSERT_TRUE(dsa_key_.ImportPublicKey(
        "AAAAB3NzaC1kc3MAAACBAPEkLhwjzIi4sr+K3CkXqwa1yk19l+ypCUv3qgWSvWi2iV07/l"
        "uvN+0kTl/Y2Kx7EWty/JUstbzTHKfqTotDnr4nu1E31s8GKNwp8hLlRmt8g+V8lcrPDXUs"
        "MUQ/O9X3B7vTRHBLYJsfhgclhZaQRGZi4bDAYYfxWL8bDMCEzwOJAAAAFQDBbNrjQ5PaXu"
        "pa2uCFUWVSbWz6YwAAAIEAhsgT8OeVOJ+G7Ph2Pj/Q329Yvnmbt0Sq8erPbxUzggJBB1kR"
        "LIc1tqBh+55VlUL2uwpMcr5rZdDxC54lPYU9XBqo2ep52MTuXudU76Uoyh9c4VeA6f7d8c"
        "JhASWEcRk6YX9prQIsBu8YbUe6TMexKJw7n25pMCL10O5tL7N7EaEAAACAEH0pD71hRBrX"
        "oCLqqa4UiBkDeImWgk5bKwufofaHnqQ2OU7wAuBV1XbO6uH/nxnfg/+CtvNpGCCDwsenCt"
        "IRZz+ajOjG33g4yD8uYjmZnCyTMNjwOyrH04FFfonWBT59a4TT0hYVhlNFtuwcdsN23vKa"
        "uIoanYu32ON72ong1OI=",
        ssh::KeyType(ssh::KeyType::DSS)));
    ASSERT_TRUE(ecdsa_key_.ImportPublicKey(
        "AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBGOhrNT2LYCXEzuRIv"
        "tx1FVYktSZqtAysuAepDu/uEHPy0GJ0qklJ/"
        "Fd53E0t2LCb07KdjPYEov4HzYs0NhezPE=",
        ssh::KeyType(ssh::KeyType::ECDSA)));
  }

  ssh::Key rsa_key_;
  ssh::Key dsa_key_;
  ssh::Key ecdsa_key_;
};

TEST_F(SSHFPRecordSetTest, GoodFingerprints) {
  const vector<string> sshfp_rrset = {
      "1 1 1B9F53A938596DF02086CC972850D50B7C65F645",
      "1 2 10AC3932B45D3C20D2E2B47708E200B0420D3C17E3937B480AAE4173 CD94B79B",
      "2 1 15D6EC062C44840BFB283EB910FBAD0B42B3E5B0",
      "2 2 B67C68E6BB1A707DCB4A773FD0DE292FF664271B51A25959C59552B4 73C09153",
      "3 1 76C7E674A84723E3B98ED6376903704ECE287BDE",
      "3 2 9AA5D6A57F6D51ECFDF7AD1C3DB3D00EB86F5CA219CACE43DC09535D 4188B765",
  };

  SSHFPRecordSet sshfp;
  ASSERT_TRUE(sshfp.Parse(sshfp_rrset));
  EXPECT_EQ(SSHFPRecordSet::Validity::VALID, sshfp.IsValid(rsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::VALID, sshfp.IsValid(dsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::VALID, sshfp.IsValid(ecdsa_key_));
}

TEST_F(SSHFPRecordSetTest, BadFingerprints) {
  const vector<string> sshfp_rrset = {
      "1 1 0B9F53A938596DF02086CC972850D50B7C65F645",
      "1 2 00AC3932B45D3C20D2E2B47708E200B0420D3C17E3937B480AAE4173 CD94B79B",
      "2 1 05D6EC062C44840BFB283EB910FBAD0B42B3E5B0",
      "2 2 067C68E6BB1A707DCB4A773FD0DE292FF664271B51A25959C59552B4 73C09153",
      "3 1 06C7E674A84723E3B98ED6376903704ECE287BDE",
      "3 2 0AA5D6A57F6D51ECFDF7AD1C3DB3D00EB86F5CA219CACE43DC09535D 4188B765",
  };

  SSHFPRecordSet sshfp;
  ASSERT_TRUE(sshfp.Parse(sshfp_rrset));
  EXPECT_EQ(SSHFPRecordSet::Validity::INVALID, sshfp.IsValid(rsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::INVALID, sshfp.IsValid(dsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::INVALID, sshfp.IsValid(ecdsa_key_));
}

TEST_F(SSHFPRecordSetTest, GoodGenericFingerprints) {
  const vector<string> sshfp_rrset = {
      // clang-format off
      "\\# 22 01011B9F53A938596DF02086CC972850D50B7C65F645",
      "\\# 34 "
        "010210AC3932B45D3C20D2E2B47708E200B0420D3C17E3937B480AAE4173CD94B79B",
      "\\# 22 020115D6EC062C44840BFB283EB910FBAD0B42B3E5B0",
      "\\# 34 "
        "0202B67C68E6BB1A707DCB4A773FD0DE292FF664271B51A25959C59552B473C09153",
      "\\# 22 030176C7E674A84723E3B98ED6376903704ECE287BDE",
      "\\# 34 "
        "03029AA5D6A57F6D51ECFDF7AD1C3DB3D00EB86F5CA219CACE43DC09535D4188B765",
      // clang-format on
  };

  SSHFPRecordSet sshfp;
  ASSERT_TRUE(sshfp.Parse(sshfp_rrset));
  EXPECT_EQ(SSHFPRecordSet::Validity::VALID, sshfp.IsValid(rsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::VALID, sshfp.IsValid(dsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::VALID, sshfp.IsValid(ecdsa_key_));
}

TEST_F(SSHFPRecordSetTest, BadGenericFingerprints) {
  const vector<string> sshfp_rrset = {
      // clang-format off
      "\\# 22 01010B9F53A938596DF02086CC972850D50B7C65F645",
      "\\# 34 "
        "010200AC3932B45D3C20D2E2B47708E200B0420D3C17E3937B480AAE4173CD94B79B",
      "\\# 22 020105D6EC062C44840BFB283EB910FBAD0B42B3E5B0",
      "\\# 34 "
        "0202067C68E6BB1A707DCB4A773FD0DE292FF664271B51A25959C59552B473C09153",
      "\\# 22 030106C7E674A84723E3B98ED6376903704ECE287BDE",
      "\\# 34 "
        "03020AA5D6A57F6D51ECFDF7AD1C3DB3D00EB86F5CA219CACE43DC09535D4188B765",
      // clang-format on
  };

  SSHFPRecordSet sshfp;
  ASSERT_TRUE(sshfp.Parse(sshfp_rrset));
  EXPECT_EQ(SSHFPRecordSet::Validity::INVALID, sshfp.IsValid(rsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::INVALID, sshfp.IsValid(dsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::INVALID, sshfp.IsValid(ecdsa_key_));
}

// At this time there is no support for SHA-256, so if only those fingerprints
// are provided, they should be considered insufficient.
TEST_F(SSHFPRecordSetTest, SHA256Fingerprints) {
  const vector<string> sshfp_rrset = {
      "1 2 10AC3932B45D3C20D2E2B47708E200B0420D3C17E3937B480AAE4173 CD94B79B",
      "2 2 B67C68E6BB1A707DCB4A773FD0DE292FF664271B51A25959C59552B4 73C09153",
      "3 2 9AA5D6A57F6D51ECFDF7AD1C3DB3D00EB86F5CA219CACE43DC09535D 4188B765",
  };

  SSHFPRecordSet sshfp;
  ASSERT_TRUE(sshfp.Parse(sshfp_rrset));
  EXPECT_EQ(SSHFPRecordSet::Validity::INSUFFICIENT, sshfp.IsValid(rsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::INSUFFICIENT, sshfp.IsValid(dsa_key_));
  EXPECT_EQ(SSHFPRecordSet::Validity::INSUFFICIENT, sshfp.IsValid(ecdsa_key_));
}
