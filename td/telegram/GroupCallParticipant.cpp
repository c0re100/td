//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2021
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "td/telegram/GroupCallParticipant.h"

#include "td/telegram/ContactsManager.h"

#include "td/utils/logging.h"

namespace td {

GroupCallParticipant::GroupCallParticipant(const tl_object_ptr<telegram_api::groupCallParticipant> &participant) {
  CHECK(participant != nullptr);
  user_id = UserId(participant->user_id_);
  audio_source = participant->source_;
  server_is_muted_by_themselves = participant->can_self_unmute_;
  server_is_muted_by_admin = participant->muted_ && !participant->can_self_unmute_;
  server_is_muted_locally = participant->muted_by_you_;
  if ((participant->flags_ & telegram_api::groupCallParticipant::VOLUME_MASK) != 0) {
    volume_level = participant->volume_;
    if (volume_level < MIN_VOLUME_LEVEL || volume_level > MAX_VOLUME_LEVEL) {
      LOG(ERROR) << "Receive " << to_string(participant);
      volume_level = 10000;
    }
    is_volume_level_local = (participant->flags_ & telegram_api::groupCallParticipant::VOLUME_BY_ADMIN_MASK) == 0;
  }
  if (!participant->left_) {
    joined_date = participant->date_;
    if ((participant->flags_ & telegram_api::groupCallParticipant::ACTIVE_DATE_MASK) != 0) {
      active_date = participant->active_date_;
    }
    if (joined_date < 0 || active_date < 0) {
      LOG(ERROR) << "Receive invalid " << to_string(participant);
      joined_date = 0;
      active_date = 0;
    }
  }
  is_just_joined = participant->just_joined_;
  is_min = (participant->flags_ & telegram_api::groupCallParticipant::MIN_MASK) != 0;
}

bool GroupCallParticipant::is_versioned_update(const tl_object_ptr<telegram_api::groupCallParticipant> &participant) {
  return participant->just_joined_ || participant->left_ || participant->versioned_;
}

bool GroupCallParticipant::get_is_muted_by_themselves() const {
  return have_pending_is_muted ? pending_is_muted_by_themselves : server_is_muted_by_themselves;
}

bool GroupCallParticipant::get_is_muted_by_admin() const {
  return have_pending_is_muted ? pending_is_muted_by_admin : server_is_muted_by_admin;
}

bool GroupCallParticipant::get_is_muted_locally() const {
  return have_pending_is_muted ? pending_is_muted_locally : server_is_muted_locally;
}

bool GroupCallParticipant::get_is_muted_for_all_users() const {
  return get_is_muted_by_admin() || get_is_muted_by_themselves();
}

int32 GroupCallParticipant::get_volume_level() const {
  return pending_volume_level != 0 ? pending_volume_level : volume_level;
}

void GroupCallParticipant::update_from(const GroupCallParticipant &old_participant) {
  CHECK(!old_participant.is_min);
  if (joined_date < old_participant.joined_date) {
    LOG(ERROR) << "Join date decreased from " << old_participant.joined_date << " to " << joined_date;
    joined_date = old_participant.joined_date;
  }
  if (active_date < old_participant.active_date) {
    active_date = old_participant.active_date;
  }
  local_active_date = old_participant.local_active_date;
  is_speaking = old_participant.is_speaking;
  if (is_min) {
    server_is_muted_locally = old_participant.server_is_muted_locally;

    if (old_participant.is_volume_level_local && !is_volume_level_local) {
      is_volume_level_local = true;
      volume_level = old_participant.volume_level;
    }
  }
  is_min = false;

  pending_volume_level = old_participant.pending_volume_level;
  pending_volume_level_generation = old_participant.pending_volume_level_generation;

  have_pending_is_muted = old_participant.have_pending_is_muted;
  pending_is_muted_by_themselves = old_participant.pending_is_muted_by_themselves;
  pending_is_muted_by_admin = old_participant.pending_is_muted_by_admin;
  pending_is_muted_locally = old_participant.pending_is_muted_locally;
  pending_is_muted_generation = old_participant.pending_is_muted_generation;
}

bool GroupCallParticipant::update_can_be_muted(bool can_manage, bool is_self, bool is_admin) {
  bool is_muted_by_admin = get_is_muted_by_admin();
  bool is_muted_by_themselves = get_is_muted_by_themselves();
  bool is_muted_locally = get_is_muted_locally();

  CHECK(!is_muted_by_admin || !is_muted_by_themselves);

  bool new_can_be_muted_for_all_users = false;
  bool new_can_be_unmuted_for_all_users = false;
  bool new_can_be_muted_only_for_self = !can_manage && !is_muted_locally;
  bool new_can_be_unmuted_only_for_self = !can_manage && is_muted_locally;
  if (is_self) {
    // current user can be muted if !is_muted_by_themselves && !is_muted_by_admin; after that is_muted_by_themselves
    // current user can be unmuted if is_muted_by_themselves; after that !is_muted
    new_can_be_muted_for_all_users = !is_muted_by_themselves && !is_muted_by_admin;
    new_can_be_unmuted_for_all_users = is_muted_by_themselves;
    new_can_be_muted_only_for_self = false;
    new_can_be_unmuted_only_for_self = false;
  } else if (is_admin) {
    // admin user can be muted if can_manage && !is_muted_by_themselves; after that is_muted_by_themselves
    // admin user can't be unmuted
    new_can_be_muted_for_all_users = can_manage && !is_muted_by_themselves;
  } else {
    // other users can be muted if can_manage && !is_muted_by_admin; after that is_muted_by_admin
    // other users can be unmuted if can_manage && is_muted_by_admin; after that is_muted_by_themselves
    new_can_be_muted_for_all_users = can_manage && !is_muted_by_admin;
    new_can_be_unmuted_for_all_users = can_manage && is_muted_by_admin;
  }
  CHECK(static_cast<int>(new_can_be_muted_for_all_users) + static_cast<int>(new_can_be_unmuted_for_all_users) +
            static_cast<int>(new_can_be_muted_only_for_self) + static_cast<int>(new_can_be_unmuted_only_for_self) <=
        1);
  if (new_can_be_muted_for_all_users != can_be_muted_for_all_users ||
      new_can_be_unmuted_for_all_users != can_be_unmuted_for_all_users ||
      new_can_be_muted_only_for_self != can_be_muted_only_for_self ||
      new_can_be_unmuted_only_for_self != can_be_unmuted_only_for_self) {
    can_be_muted_for_all_users = new_can_be_muted_for_all_users;
    can_be_unmuted_for_all_users = new_can_be_unmuted_for_all_users;
    can_be_muted_only_for_self = new_can_be_muted_only_for_self;
    can_be_unmuted_only_for_self = new_can_be_unmuted_only_for_self;
    return true;
  }
  return false;
}

bool GroupCallParticipant::set_pending_is_muted(bool is_muted, bool can_manage, bool is_self, bool is_admin) {
  update_can_be_muted(can_manage, is_self, is_admin);
  if (is_muted) {
    if (!can_be_muted_for_all_users && !can_be_muted_only_for_self) {
      return false;
    }
    CHECK(!can_be_muted_for_all_users || !can_be_muted_only_for_self);
  } else {
    if (!can_be_unmuted_for_all_users && !can_be_unmuted_only_for_self) {
      return false;
    }
    CHECK(!can_be_unmuted_for_all_users || !can_be_unmuted_only_for_self);
  }

  if (is_self) {
    pending_is_muted_by_themselves = is_muted;
    pending_is_muted_by_admin = false;
    pending_is_muted_locally = false;
  } else {
    pending_is_muted_by_themselves = get_is_muted_by_themselves();
    pending_is_muted_by_admin = get_is_muted_by_admin();
    pending_is_muted_locally = get_is_muted_locally();
    if (is_muted) {
      if (can_be_muted_only_for_self) {
        // local mute
        pending_is_muted_locally = true;
      } else {
        // admin mute
        CHECK(can_be_muted_for_all_users);
        CHECK(can_manage);
        if (is_admin) {
          CHECK(!pending_is_muted_by_themselves);
          pending_is_muted_by_admin = false;
          pending_is_muted_by_themselves = true;
        } else {
          CHECK(!pending_is_muted_by_admin);
          pending_is_muted_by_admin = true;
          pending_is_muted_by_themselves = false;
        }
      }
    } else {
      if (can_be_unmuted_only_for_self) {
        // local unmute
        pending_is_muted_locally = false;
      } else {
        // admin unmute
        CHECK(can_be_unmuted_for_all_users);
        CHECK(can_manage);
        CHECK(!is_admin);
        pending_is_muted_by_admin = false;
        pending_is_muted_by_themselves = true;
      }
    }
  }

  have_pending_is_muted = true;
  update_can_be_muted(can_manage, is_self, is_admin);
  return true;
}

td_api::object_ptr<td_api::groupCallParticipant> GroupCallParticipant::get_group_call_participant_object(
    ContactsManager *contacts_manager) const {
  if (!is_valid()) {
    return nullptr;
  }

  return td_api::make_object<td_api::groupCallParticipant>(
      contacts_manager->get_user_id_object(user_id, "get_group_call_participant_object"), audio_source, is_speaking,
      can_be_muted_for_all_users, can_be_unmuted_for_all_users, can_be_muted_only_for_self,
      can_be_unmuted_only_for_self, get_is_muted_for_all_users(), get_is_muted_locally(), get_is_muted_by_themselves(),
      get_volume_level(), order);
}

bool operator==(const GroupCallParticipant &lhs, const GroupCallParticipant &rhs) {
  return lhs.user_id == rhs.user_id && lhs.audio_source == rhs.audio_source &&
         lhs.can_be_muted_for_all_users == rhs.can_be_muted_for_all_users &&
         lhs.can_be_unmuted_for_all_users == rhs.can_be_unmuted_for_all_users &&
         lhs.can_be_muted_only_for_self == rhs.can_be_muted_only_for_self &&
         lhs.can_be_unmuted_only_for_self == rhs.can_be_unmuted_only_for_self &&
         lhs.get_is_muted_for_all_users() == rhs.get_is_muted_for_all_users() &&
         lhs.get_is_muted_locally() == rhs.get_is_muted_locally() &&
         lhs.get_is_muted_by_themselves() == rhs.get_is_muted_by_themselves() && lhs.is_speaking == rhs.is_speaking &&
         lhs.get_volume_level() == rhs.get_volume_level() && lhs.order == rhs.order;
}

bool operator!=(const GroupCallParticipant &lhs, const GroupCallParticipant &rhs) {
  return !(lhs == rhs);
}

StringBuilder &operator<<(StringBuilder &string_builder, const GroupCallParticipant &group_call_participant) {
  return string_builder << '[' << group_call_participant.user_id << " with source "
                        << group_call_participant.audio_source << " and order " << group_call_participant.order << ']';
}

}  // namespace td