// Copyright 2016-2018 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef READER_INFO_HPP_
#define READER_INFO_HPP_

#include <cassert>
#include <map>
#include <utility>
#include <string>
#include <vector>

#include "rcutils/logging_macros.h"

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "fastrtps/participant/Participant.h"
#include "fastrtps/rtps/builtin/data/ReaderProxyData.h"
#include "fastrtps/rtps/reader/ReaderListener.h"
#include "fastrtps/rtps/reader/RTPSReader.h"

#include "types/guard_condition.hpp"
#include "types/topic_cache.hpp"

class ReaderInfo : public eprosima::fastrtps::rtps::ReaderListener
{
public:
  ReaderInfo(
    eprosima::fastrtps::Participant * participant,
    rmw_guard_condition_t * graph_guard_condition)
  : participant_(participant),
    graph_guard_condition_(static_cast<GuardCondition *>(graph_guard_condition->data))
  {}

  void
  onNewCacheChangeAdded(
    eprosima::fastrtps::rtps::RTPSReader *,
    const eprosima::fastrtps::rtps::CacheChange_t * const change)
  {
    eprosima::fastrtps::rtps::ReaderProxyData proxyData;
    if (eprosima::fastrtps::rtps::ALIVE == change->kind) {
      eprosima::fastrtps::rtps::CDRMessage_t tempMsg(0);
      tempMsg.wraps = true;
      if (PL_CDR_BE == change->serializedPayload.encapsulation) {
        tempMsg.msg_endian = eprosima::fastrtps::rtps::BIGEND;
      } else {
        tempMsg.msg_endian = eprosima::fastrtps::rtps::LITTLEEND;
      }
      tempMsg.length = change->serializedPayload.length;
      tempMsg.max_size = change->serializedPayload.max_size;
      tempMsg.buffer = change->serializedPayload.data;
      if (!proxyData.readFromCDRMessage(&tempMsg)) {
        return;
      }
    } else {
      GUID_t readerGuid;
      iHandle2GUID(readerGuid, change->instanceHandle);
      if (!participant_->get_remote_reader_info(readerGuid, proxyData)) {
        return;
      }
    }

    bool trigger = false;
    {
      std::lock_guard<std::mutex> guard(topic_cache_.getMutex());
      if (eprosima::fastrtps::rtps::ALIVE == change->kind) {
        trigger = topic_cache_.addTopic(proxyData.RTPSParticipantKey(),
                proxyData.topicName(), proxyData.typeName());
      } else {
        trigger = topic_cache_.removeTopic(proxyData.RTPSParticipantKey(),
                                        proxyData.topicName(), proxyData.typeName());
      }
    }

    if (trigger) {
      graph_guard_condition_->trigger();
    }
  }

  LockedObject<TopicCache> topic_cache_;
  eprosima::fastrtps::Participant * participant_;
  GuardCondition * graph_guard_condition_;
};

#endif  // READER_INFO_HPP_
