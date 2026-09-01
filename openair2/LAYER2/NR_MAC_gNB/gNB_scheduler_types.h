/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file gNB_scheduler_types.h
 * \brief Scheduler algorithm type definitions
 * \author
 * \date 2024
 * \version 1.0
 * @ingroup _mac
 */

#ifndef GNB_SCHEDULER_TYPES_H__
#define GNB_SCHEDULER_TYPES_H__

/*!
 * \brief Scheduler algorithm types
 */
typedef enum {
  SCHE_PF = 0,  /*!< Proportional Fair — full BWP via nr_*_schedule(slice_prb=NULL) */
  SCHE_NS = 1   /*!< Network slicing — slice_prb_allocator then per-slice nr_*_schedule() */
} scheduler_type_t;

#endif /* GNB_SCHEDULER_TYPES_H__ */
