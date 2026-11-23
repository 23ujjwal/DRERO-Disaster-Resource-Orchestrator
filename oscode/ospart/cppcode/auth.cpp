#include "auth.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <sstream>
#include <vector>
#include <stdexcept>
namespace {
std::string hex(const unsigned char *data, size_t size) {
    const char *digits = "0123456789abcdef";
    std::string out;
    for (size_t i=0;i<size;i++) { out += digits[data[i]>>4]; out += digits[data[i]&15]; }
    return out;
}
std::vector<unsigned char> unhex(const std::string &s) {
    if (s.size()%2) throw std::runtime_error("Invalid encoding");
    std::vector<unsigned char> out;
    for(size_t i=0;i<s.size();i+=2) {
        auto nibble=[](char c)->int { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; throw std::runtime_error("Invalid encoding"); };
        out.push_back((nibble(s[i])<<4)|nibble(s[i+1]));
    }
    return out;
}
std::string derive(const std::string &password, const std::vector<unsigned char> &salt, int iterations) {
    unsigned char out[32];
    if(!PKCS5_PBKDF2_HMAC(password.data(), password.size(),salt.data(),salt.size(),iterations,EVP_sha256(),sizeof(out),out))
        throw std::runtime_error("Password derivation failed");
    return hex(out,sizeof(out));
}
}
namespace drero_auth {
std::string random_token() {
    unsigned char data[32];
    if(RAND_bytes(data,sizeof(data))!=1)throw std::runtime_error("Secure random generation failed");
    return hex(data,sizeof(data));
}
std::string token_hash(const std::string &token) {
    unsigned char out[EVP_MAX_MD_SIZE]; unsigned int size=0;
    if(EVP_Digest(token.data(),token.size(),out,&size,EVP_sha256(),nullptr)!=1)throw std::runtime_error("Token hashing failed");
    return hex(out,size);
}
bool is_password_hash(const std::string &stored) { return stored.rfind("pbkdf2_sha256$",0)==0; }
std::string hash_password(const std::string &password) {
    unsigned char salt[16];
    if(RAND_bytes(salt,sizeof(salt))!=1)throw std::runtime_error("Secure random generation failed");
    return "pbkdf2_sha256$600000$"+hex(salt,sizeof(salt))+"$"+derive(password,{salt,salt+sizeof(salt)},600000);
}
bool verify_password(const std::string &password,const std::string &stored) {
    if(password.size()>1024 || !is_password_hash(stored))return false;
    try {
        std::istringstream stream(stored); std::string part; std::vector<std::string> parts;
        while(std::getline(stream,part,'$'))parts.push_back(part);
        if(parts.size()!=4 || parts[1]!="600000" || parts[2].size()!=32 || parts[3].size()!=64)return false;
        auto digest=derive(password,unhex(parts[2]),600000);
        return CRYPTO_memcmp(digest.data(),parts[3].data(),digest.size())==0;
    } catch(...) { return false; }
}
}
