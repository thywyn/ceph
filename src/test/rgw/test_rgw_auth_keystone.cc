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
  EXPECT_EQ("evil_inject_attack", sanitize_name("evil$inject$attack"));
  EXPECT_EQ("null_byte", sanitize_name(std::string("null\0byte", 9)));
  EXPECT_EQ("project_user", sanitize_name("project$user"));
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

// Test construct_rgw_user function
TEST(KeystoneAuthTest, ConstructRgwUserBasic)
{
  rgw::keystone::TokenEnvelope token;

  // Set up a mock token with project and user
  token.project.id = "project456";
  token.project.name = "team-backend";
  token.user.id = "user789";
  token.user.name = "alice";

  rgw_user user = construct_rgw_user(token);
  EXPECT_EQ("team-backend$alice", user.to_str());
  EXPECT_EQ("team-backend", user.tenant);
  EXPECT_EQ("alice", user.id);
}

TEST(KeystoneAuthTest, ConstructRgwUserMultipleUsers)
{
  rgw::keystone::TokenEnvelope token1, token2;

  // Same project, different users
  token1.project.name = "team-backend";
  token1.user.name = "alice";

  token2.project.name = "team-backend";
  token2.user.name = "bob";

  rgw_user user1 = construct_rgw_user(token1);
  rgw_user user2 = construct_rgw_user(token2);

  EXPECT_EQ("team-backend$alice", user1.to_str());
  EXPECT_EQ("team-backend$bob", user2.to_str());
  EXPECT_NE(user1.to_str(), user2.to_str());
}

TEST(KeystoneAuthTest, ConstructRgwUserDifferentProjects)
{
  rgw::keystone::TokenEnvelope token1, token2;

  // Different projects, same user name
  token1.project.name = "project1";
  token1.user.name = "alice";

  token2.project.name = "project2";
  token2.user.name = "alice";

  rgw_user user1 = construct_rgw_user(token1);
  rgw_user user2 = construct_rgw_user(token2);

  EXPECT_EQ("project1$alice", user1.to_str());
  EXPECT_EQ("project2$alice", user2.to_str());
  EXPECT_NE(user1.to_str(), user2.to_str());
}

TEST(KeystoneAuthTest, ConstructRgwUserSanitization)
{
  rgw::keystone::TokenEnvelope token;

  // Names with special characters should be sanitized
  token.project.name = "bad$project";
  token.user.name = "user@host";

  rgw_user user = construct_rgw_user(token);
  std::string user_str = user.to_str();

  EXPECT_EQ("bad_project$user_host", user_str);
  EXPECT_EQ("bad_project", user.tenant);
  EXPECT_EQ("user_host", user.id);

  // Ensure exactly one dollar sign delimiter
  EXPECT_NE(std::string::npos, user_str.find('$'));  // Should have one $

  // Count delimiters (should be exactly 1)
  size_t dollar_count = std::count(user_str.begin(), user_str.end(), '$');
  EXPECT_EQ(1u, dollar_count);
}

TEST(KeystoneAuthTest, ConstructRgwUserFallbackToIds)
{
  rgw::keystone::TokenEnvelope token;

  // If names are empty, should fall back to IDs
  token.project.id = "project-id-456";
  token.project.name = "";
  token.user.id = "user-id-789";
  token.user.name = "";

  rgw_user user = construct_rgw_user(token);
  EXPECT_EQ("project-id-456$user-id-789", user.to_str());
  EXPECT_EQ("project-id-456", user.tenant);
  EXPECT_EQ("user-id-789", user.id);
}

TEST(KeystoneAuthTest, ConstructRgwUserRealWorldExamples)
{
  rgw::keystone::TokenEnvelope token;

  // Test case from implementation
  token.project.name = "team-backend";
  token.user.name = "alice";

  rgw_user user1 = construct_rgw_user(token);
  EXPECT_EQ("team-backend$alice", user1.to_str());

  // Another test case
  token.project.name = "team-west";
  token.user.name = "dave";

  rgw_user user2 = construct_rgw_user(token);
  EXPECT_EQ("team-west$dave", user2.to_str());
}
