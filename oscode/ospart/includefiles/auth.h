#pragma once
#include <string>
namespace drero_auth {
std::string random_token();
std::string token_hash(const std::string &token);
std::string hash_password(const std::string &password);
bool verify_password(const std::string &password, const std::string &stored);
bool is_password_hash(const std::string &stored);
}
