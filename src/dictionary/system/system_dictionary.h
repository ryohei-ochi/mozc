// Copyright 2010-2015, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Mozc system dictionary

#ifndef MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
#define MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/system/codec_interface.h"
#include "dictionary/system/key_expansion_table.h"
#include "dictionary/system/words_info.h"
#include "storage/louds/bit_vector_based_array.h"
#include "storage/louds/louds_trie.h"
// for FRIEND_TEST
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class DictionaryFile;
struct Token;

namespace dictionary {

class SystemDictionaryCodecInterface;
class ReverseLookupIndex;

class SystemDictionary : public DictionaryInterface {
 public:
  // System dictionary options represented as bitwise enum.
  enum Options {
    NONE = 0,
    // If ENABLE_REVERSE_LOOKUP_INDEX is set, we will have the index in heap
    // from the id in value trie to the id in key trie.
    // That consumes more memory but we can perform reverse lookup more quickly.
    ENABLE_REVERSE_LOOKUP_INDEX = 1,
  };

  // Builder class for system dictionary
  // Usage:
  //   SystemDictionary::Builder builder(filename);
  //   builder.SetOptions(SystemDictionary::NONE);
  //   builder.SetCodec(NULL);
  //   SystemDictionary *dictionry = builder.Build();
  class Builder {
   public:
    // Creates Builder from filename
    explicit Builder(const string &filename);
    // Creates Builder from image
    Builder(const char *ptr, int len);
    ~Builder();

    // Sets options (default: NONE)
    void SetOptions(Options options);

    // Sets codec (default: NULL)
    // Uses default codec if this is NULL
    void SetCodec(const SystemDictionaryCodecInterface *codec);

    // Builds and returns system dictionary.
    SystemDictionary *Build();

   private:
    enum InputType {
      FILENAME,
      IMAGE,
    };

    InputType type_;

    // For InputType::FILENAME
    const string filename_;

    // For InputTYpe::IMAGE
    const char *ptr_;
    const int len_;

    Options options_;
    const SystemDictionaryCodecInterface *codec_;

    DISALLOW_COPY_AND_ASSIGN(Builder);
  };

  struct ReverseLookupResult {
    ReverseLookupResult() : tokens_offset(-1), id_in_key_trie(-1) {}
    // Offset from the tokens section beginning.
    // (token_array_->Get(id_in_key_trie) ==
    //  token_array_->Get(0) + tokens_offset)
    int tokens_offset;
    // Id in key trie
    int id_in_key_trie;
  };

  virtual ~SystemDictionary();

  // TODO(team): Use builder instead of following static methods.
  static SystemDictionary *CreateSystemDictionaryFromFile(
      const string &filename);

  static SystemDictionary *CreateSystemDictionaryFromFileWithOptions(
      const string &filename, Options options);

  static SystemDictionary *CreateSystemDictionaryFromImage(
      const char *ptr, int len);

  static SystemDictionary *CreateSystemDictionaryFromImageWithOptions(
      const char *ptr, int len, Options options);

  // Implementation of DictionaryInterface.
  virtual bool HasKey(StringPiece key) const;
  virtual bool HasValue(StringPiece value) const;
  virtual void LookupPredictive(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const;
  virtual void LookupPrefix(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const;
  virtual void LookupExact(StringPiece key, Callback *callback) const;
  virtual void LookupReverse(StringPiece str, NodeAllocatorInterface *allocator,
                             Callback *callback) const;
  virtual void PopulateReverseLookupCache(
      StringPiece str, NodeAllocatorInterface *allocator) const;
  virtual void ClearReverseLookupCache(
      NodeAllocatorInterface *allocator) const;

 private:
  FRIEND_TEST(SystemDictionaryTest, TokenAfterSpellningToken);

  struct FilterInfo {
    enum Condition {
      NONE = 0,
      VALUE_ID = 1,
      NO_SPELLING_CORRECTION = 2,
      ONLY_T13N = 4,
    };
    int conditions;
    // Return results only for tokens with given |value_id|.
    // If VALUE_ID is specified
    int value_id;
    FilterInfo() : conditions(NONE), value_id(-1) {}
  };

  explicit SystemDictionary(const SystemDictionaryCodecInterface *codec);

  bool OpenDictionaryFile(bool enable_reverse_lookup_index);

  // Calls |callback| with token info, which is filled using |tokens_key|,
  // |actual_key| and |encoded_tokens_ptr|.
  // |tokens_key| is a key used for look up.
  // |actual_key| is a token's key.
  // They may be different when we perform ambiguous search.
  void RegisterTokens(
      const FilterInfo &filter,
      const string &tokens_key,
      const string &actual_key,
      const uint8 *encoded_tokens_ptr,
      Callback *callback) const;

  bool IsBadToken(const FilterInfo &filter, const TokenInfo &token_info) const;

  void RegisterReverseLookupTokensForT13N(StringPiece value,
                                          Callback *callback) const;

  void RegisterReverseLookupTokensForValue(StringPiece value,
                                           NodeAllocatorInterface *allocator,
                                           Callback *callback) const;

  void ScanTokens(const set<int> &id_set,
                  multimap<int, ReverseLookupResult> *reverse_results) const;

  void RegisterReverseLookupResults(
      const set<int> &id_set,
      const multimap<int, ReverseLookupResult> &reverse_results,
      Callback *callback) const;

  void InitReverseLookupIndex();

  Callback::ResultType LookupPrefixWithKeyExpansionImpl(
      const char *key,
      StringPiece encoded_key,
      const KeyExpansionTable &table,
      Callback *callback,
      storage::louds::LoudsTrie::Node node,
      StringPiece::size_type key_pos,
      bool is_expanded,
      char *actual_key_buffer,
      string *actual_prefix) const;

  struct PredictiveLookupSearchState {
    PredictiveLookupSearchState() : key_pos(0), is_expanded(false) {}
    PredictiveLookupSearchState(const storage::louds::LoudsTrie::Node &n,
                                size_t pos, bool expanded)
        : node(n), key_pos(pos), is_expanded(expanded) {}

    storage::louds::LoudsTrie::Node node;
    size_t key_pos;
    bool is_expanded;
  };

  void CollectPredictiveNodesInBfsOrder(
      StringPiece encoded_key,
      const KeyExpansionTable &table,
      size_t limit,
      vector<PredictiveLookupSearchState> *result) const;

  storage::louds::LoudsTrie key_trie_;
  storage::louds::LoudsTrie value_trie_;
  storage::louds::BitVectorBasedArray token_array_;
  const uint32 *frequent_pos_;
  const SystemDictionaryCodecInterface *codec_;
  KeyExpansionTable hiragana_expansion_table_;
  scoped_ptr<DictionaryFile> dictionary_file_;
  scoped_ptr<ReverseLookupIndex> reverse_lookup_index_;

  DISALLOW_COPY_AND_ASSIGN(SystemDictionary);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_SYSTEM_DICTIONARY_H_
