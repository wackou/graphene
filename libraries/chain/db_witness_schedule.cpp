/*
 * Copyright (c) 2015, Cryptonomex, Inc.
 * All rights reserved.
 *
 * This source code is provided for evaluation in private test networks only, until September 8, 2015. After this date, this license expires and
 * the code may not be used, modified or distributed for any purpose. Redistribution and use in source and binary forms, with or without modification,
 * are permitted until September 8, 2015, provided that the following conditions are met:
 *
 * 1. The code and/or derivative works are used only for private test networks consisting of no more than 10 P2P nodes.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#include <graphene/chain/database.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/witness_schedule_object.hpp>

namespace graphene { namespace chain {

using boost::container::flat_set;

witness_id_type database::get_scheduled_witness( uint32_t slot_num )const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   const witness_schedule_object& wso = witness_schedule_id_type()(*this);
   uint64_t current_aslot = dpo.current_aslot + slot_num;
   return wso.current_shuffled_witnesses[ current_aslot % wso.current_shuffled_witnesses.size() ];
}

fc::time_point_sec database::get_slot_time(uint32_t slot_num)const
{
   if( slot_num == 0 )
      return fc::time_point_sec();

   auto interval = block_interval();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   if( head_block_num() == 0 )
   {
      // n.b. first block is at genesis_time plus one block interval
      fc::time_point_sec genesis_time = dpo.time;
      return genesis_time + slot_num * interval;
   }

   auto head_block_abs_slot = head_block_time().sec_since_epoch() / interval;
   fc::time_point_sec head_slot_time(head_block_abs_slot * interval);

   const global_property_object& gpo = get_global_properties();

   // "slot 0" is head_slot_time
   // "slot 1" is head_slot_time,
   //   plus maint interval if head block is a maint block
   //   plus block interval if head block is not a maint block
   return head_slot_time
        + (slot_num +
           (
            (dpo.dynamic_flags & dynamic_global_property_object::maintenance_flag)
            ? gpo.parameters.maintenance_skip_slots : 0
           )
          ) * interval
        ;
}

uint32_t database::get_slot_at_time(fc::time_point_sec when)const
{
   fc::time_point_sec first_slot_time = get_slot_time( 1 );
   if( when < first_slot_time )
      return 0;
   return (when - first_slot_time).to_seconds() / block_interval() + 1;
}

uint32_t database::witness_participation_rate()const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   return uint64_t(GRAPHENE_100_PERCENT) * dpo.recent_slots_filled.popcount() / 128;
}

void database::update_witness_schedule()
{
   const witness_schedule_object& wso = witness_schedule_id_type()(*this);
   const global_property_object& gpo = get_global_properties();

   if( head_block_num() % gpo.active_witnesses.size() == 0 )
   {
      modify( wso, [&]( witness_schedule_object& _wso )
      {
         _wso.current_shuffled_witnesses.clear();
         _wso.current_shuffled_witnesses.reserve( gpo.active_witnesses.size() );

         for( const witness_id_type& w : gpo.active_witnesses )
            _wso.current_shuffled_witnesses.push_back( w );

         auto now_hi = uint64_t(head_block_time().sec_since_epoch()) << 32;
         for( uint32_t i = 0; i < _wso.current_shuffled_witnesses.size(); ++i )
         {
            /// High performance random generator
            /// http://xorshift.di.unimi.it/
            uint64_t k = now_hi + uint64_t(i)*2685821657736338717ULL;
            k ^= (k >> 12);
            k ^= (k << 25);
            k ^= (k >> 27);
            k *= 2685821657736338717ULL;

            uint32_t jmax = _wso.current_shuffled_witnesses.size() - i;
            uint32_t j = i + k%jmax;
            std::swap( _wso.current_shuffled_witnesses[i],
                       _wso.current_shuffled_witnesses[j] );
         }
      });
   }
}

} }
