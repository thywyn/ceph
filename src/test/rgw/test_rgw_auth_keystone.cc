// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:nil -*-
// vim: ts=8 sw=2 sts=2 expandtab

/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2024 Red Hat
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */

#include "rgw_auth_keystone.h"
#include "rgw_keystone.h"
#include <gtest/gtest.h>

using namespace rgw::auth::keystone;

// Test sanitize_name function
TEST(KeystoneAuthTest, SanitizeNameValid)
{
  // Valid names should pass through unchanged
  EXPECT_EQ("alice", sanitize_name("alice"));
  EXPECT_EQ("bob-123", sanitize_name("bob-123"));
  EXPECT_EQ("user_name", sanitize_name("user_name"));
  EXPECT_EQ("Test.User", sanitize_name("Test.User"));
  EXPECT_EQ("a1b2c3", sanitize_name("a1b2c3"));
}

TEST(KeystoneAuthTest, SanitizeNameInjectionPrevention)
{
  // Dangerous characters should be replaced with underscores
  EXPECT_EQ("evil_inject_attack", sanitize_name("evil:inject:attack"));
  EXPECT_EQ("null_byte", sanitize_name(std::string("null\0byte", 9)));
  EXPECT_EQ("project_user", sanitize_name("project:user"));
}

TEST(KeystoneAuthTest, SanitizeNameSpecialCharacters)
{
  // Other special characters should be replaced
  EXPECT_EQ("user_name", sanitize_name("user@name"));
  EXPECT_EQ("user_name", sanitize_name("user#name"));
  EXPECT_EQ("user_name", sanitize_name("user name"));
  EXPECT_EQ("user_name__", sanitize_name("user/name\\|"));
}

TEST(KeystoneAuthTest, SanitizeNameLength)
{
  // Very long names should be truncated to 80 chars
  std::string long_name(100, 'a');
  std::string sanitized = sanitize_name(long_name);
  EXPECT_EQ(80u, sanitized.size());
  EXPECT_EQ(std::string(80, 'a'), sanitized);
}

TEST(KeystoneAuthTest, SanitizeNameEmpty)
{
  // Empty string should remain empty
  EXPECT_EQ("", sanitize_name(""));
}

// Test construct_user_id function
TEST(KeystoneAuthTest, ConstructUserIdBasic)
{
  rgw::keystone::TokenEnvelope token;

  // Set up a mock token with project and user
  token.project.id = "project456";
  token.project.name = "team-backend";
  token.user.id = "user789";
  token.user.name = "alice";

  std::string user_id = construct_user_id(token);
  EXPECT_EQ("team-backend:alice", user_id);
}

TEST(KeystoneAuthTest, ConstructUserIdMultipleUsers)
{
  rgw::keystone::TokenEnvelope token1, token2;

  // Same project, different users
  token1.project.name = "team-backend";
  token1.user.name = "alice";

  token2.project.name = "team-backend";
  token2.user.name = "bob";

  EXPECT_EQ("team-backend:alice", construct_user_id(token1));
  EXPECT_EQ("team-backend:bob", construct_user_id(token2));
  EXPECT_NE(construct_user_id(token1), construct_user_id(token2));
}

TEST(KeystoneAuthTest, ConstructUserIdDifferentProjects)
{
  rgw::keystone::TokenEnvelope token1, token2;

  // Different projects, same user
  token1.project.name = "project1";
  token1.user.name = "alice";

  token2.project.name = "project2";
  token2.user.name = "alice";

  EXPECT_EQ("project1:alice", construct_user_id(token1));
  EXPECT_EQ("project2:alice", construct_user_id(token2));
  EXPECT_NE(construct_user_id(token1), construct_user_id(token2));
}

TEST(KeystoneAuthTest, ConstructUserIdSanitization)
{
  rgw::keystone::TokenEnvelope token;

  // Names with special characters should be sanitized
  token.project.name = "bad:project";
  token.user.name = "user@host";

  std::string user_id = construct_user_id(token);
  EXPECT_EQ("bad_project:user_host", user_id);

  // Ensure exactly one colon delimiter
  EXPECT_NE(std::string::npos, user_id.find(':'));  // Should have one :

  // Count delimiters (should be exactly 1)
  size_t colon_count = std::count(user_id.begin(), user_id.end(), ':');
  EXPECT_EQ(1u, colon_count);
}

TEST(KeystoneAuthTest, ConstructUserIdFallbackToIds)
{
  rgw::keystone::TokenEnvelope token;

  // If names are empty, should fall back to IDs
  token.project.id = "project-id-456";
  token.project.name = "";
  token.user.id = "user-id-789";
  token.user.name = "";

  std::string user_id = construct_user_id(token);
  EXPECT_EQ("project-id-456:user-id-789", user_id);
}

TEST(KeystoneAuthTest, ConstructUserIdRealWorldExamples)
{
  rgw::keystone::TokenEnvelope token;

  // Test case from implementation
  token.project.name = "team-backend";
  token.user.name = "alice";

  EXPECT_EQ("team-backend:alice", construct_user_id(token));

  // Another test case
  token.project.name = "team-west";
  token.user.name = "dave";

  EXPECT_EQ("team-west:dave", construct_user_id(token));
}
