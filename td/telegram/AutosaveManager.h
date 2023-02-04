//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2023
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/telegram/DialogId.h"
#include "td/telegram/td_api.h"
#include "td/telegram/telegram_api.h"

#include "td/actor/actor.h"

#include "td/utils/common.h"
#include "td/utils/FlatHashMap.h"
#include "td/utils/Promise.h"
#include "td/utils/Status.h"

namespace td {

class Td;

class AutosaveManager final : public Actor {
 public:
  AutosaveManager(Td *td, ActorShared<> parent);

  void reload_autosave_settings(Promise<td_api::object_ptr<td_api::autosaveSettings>> &&promise);

  void get_autosave_settings(Promise<td_api::object_ptr<td_api::autosaveSettings>> &&promise);

  void set_autosave_settings(td_api::object_ptr<td_api::AutosaveSettingsScope> &&scope,
                             td_api::object_ptr<td_api::scopeAutosaveSettings> &&settings, Promise<Unit> &&promise);

  void clear_autosave_settings_excpetions(Promise<Unit> &&promise);

 private:
  struct DialogAutosaveSettings {
    bool are_inited_ = false;
    bool autosave_photos_ = false;
    bool autosave_videos_ = false;
    int64 max_video_file_size_ = 0;

    static constexpr int64 MIN_MAX_VIDEO_FILE_SIZE = 512 * 1024;
    static constexpr int64 DEFAULT_MAX_VIDEO_FILE_SIZE = 100 * 1024 * 1024;
    static constexpr int64 MAX_MAX_VIDEO_FILE_SIZE = static_cast<int64>(4000) * 1024 * 1024;

    DialogAutosaveSettings() = default;

    explicit DialogAutosaveSettings(const telegram_api::autoSaveSettings *settings);

    explicit DialogAutosaveSettings(const td_api::scopeAutosaveSettings *settings);

    telegram_api::object_ptr<telegram_api::autoSaveSettings> get_input_auto_save_settings() const;

    td_api::object_ptr<td_api::scopeAutosaveSettings> get_scope_autosave_settings_object() const;

    td_api::object_ptr<td_api::autosaveSettingsException> get_autosave_settings_exception_object(
        DialogId dialog_id) const;

    bool operator==(const DialogAutosaveSettings &other) const;

    bool operator!=(const DialogAutosaveSettings &other) const;
  };

  struct AutosaveSettings {
    bool are_inited_ = false;
    DialogAutosaveSettings user_settings_;
    DialogAutosaveSettings chat_settings_;
    DialogAutosaveSettings broadcast_settings_;
    FlatHashMap<DialogId, DialogAutosaveSettings, DialogIdHash> exceptions_;

    td_api::object_ptr<td_api::autosaveSettings> get_autosave_settings_object() const;
  };

  void tear_down() final;

  void on_get_autosave_settings(Result<telegram_api::object_ptr<telegram_api::account_autoSaveSettings>> r_settings);

  static td_api::object_ptr<td_api::updateAutosaveSettings> get_update_autosave_settings(
      td_api::object_ptr<td_api::AutosaveSettingsScope> &&scope, const DialogAutosaveSettings &settings);

  void send_update_autosave_settings(td_api::object_ptr<td_api::AutosaveSettingsScope> &&scope,
                                     const DialogAutosaveSettings &settings);

  Td *td_;
  ActorShared<> parent_;

  AutosaveSettings settings_;
  vector<Promise<td_api::object_ptr<td_api::autosaveSettings>>> load_settings_queries_;
};

}  // namespace td
