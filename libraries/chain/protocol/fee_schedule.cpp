#include <graphene/chain/protocol/fee_schedule.hpp>
#include <fc/smart_ref_impl.hpp>

namespace fc
{
   // explicitly instantiate the smart_ref, gcc fails to instantiate it in some release builds
   //template graphene::chain::fee_schedule& smart_ref<graphene::chain::fee_schedule>::operator=(smart_ref<graphene::chain::fee_schedule>&&);
   //template graphene::chain::fee_schedule& smart_ref<graphene::chain::fee_schedule>::operator=(U&&);
   //template graphene::chain::fee_schedule& smart_ref<graphene::chain::fee_schedule>::operator=(const smart_ref&);
   //template smart_ref<graphene::chain::fee_schedule>::smart_ref();
   //template const graphene::chain::fee_schedule& smart_ref<graphene::chain::fee_schedule>::operator*() const;
}

namespace graphene { namespace chain {

   typedef fc::smart_ref<fee_schedule> smart_fee_schedule;

   fee_schedule::fee_schedule()
   {
   }

   fee_schedule fee_schedule::get_default()
   {
      fee_schedule result;
      for( int i = 0; i < fee_parameters().count(); ++i )
      {
         fee_parameters x; x.set_which(i);
         result.parameters.insert(x);
      }
      return result;
   }

   struct fee_schedule_validate_visitor
   {
      typedef void result_type;

      template<typename T>
      void operator()( const T& p )const
      {
         //p.validate();
      }
   };

   void fee_schedule::validate()const
   {
      for( const auto& f : parameters )
         f.visit( fee_schedule_validate_visitor() );
   }

   struct calc_fee_visitor
   {
      typedef uint64_t result_type;

      const fee_parameters& param;
      calc_fee_visitor( const fee_parameters& p ):param(p){}

      template<typename OpType>
      result_type operator()(  const OpType& op )const
      {
         return op.calculate_fee( param.get<typename OpType::fee_parameters_type>() ).value;
      }
   };

   struct set_fee_visitor
   {
      typedef void result_type;
      asset _fee;

      set_fee_visitor( asset f ):_fee(f){}

      template<typename OpType>
      void operator()( OpType& op )const
      {
         op.fee = _fee;
      }
   };

   struct zero_fee_visitor
   {
      typedef void result_type;

      template<typename ParamType>
      result_type operator()(  ParamType& op )const
      {
         memset( (char*)&op, 0, sizeof(op) );
      }
   };

   void fee_schedule::zero_all_fees()
   {
      *this = get_default();
      for( fee_parameters& i : parameters )
         i.visit( zero_fee_visitor() );
      this->scale = 0;
   }

   asset fee_schedule::calculate_fee( const operation& op, const price& core_exchange_rate )const
   {
      //idump( (op)(core_exchange_rate) );
      fee_parameters params; params.set_which(op.which());
      auto itr = parameters.find(params);
      if( itr != parameters.end() ) params = *itr;
      auto base_value = op.visit( calc_fee_visitor( params ) );
      auto scaled = fc::uint128(base_value) * scale;
      scaled /= GRAPHENE_100_PERCENT;
      FC_ASSERT( scaled <= GRAPHENE_MAX_SHARE_SUPPLY );
      //idump( (base_value)(scaled)(core_exchange_rate) );
      auto result = asset( scaled.to_uint64(), 0 ) * core_exchange_rate;
      //FC_ASSERT( result * core_exchange_rate >= asset( scaled.to_uint64()) );

      while( result * core_exchange_rate < asset( scaled.to_uint64()) )
        result.amount++;

      FC_ASSERT( result.amount <= GRAPHENE_MAX_SHARE_SUPPLY );
      return result;
   }

   asset fee_schedule::set_fee( operation& op, const price& core_exchange_rate )const
   {
      auto f = calculate_fee( op, core_exchange_rate );
      op.visit( set_fee_visitor( f ) );
      return f;
   }

   void chain_parameters::validate()const
   {
      current_fees->validate();
      FC_ASSERT( reserve_percent_of_fee <= GRAPHENE_100_PERCENT );
      FC_ASSERT( network_percent_of_fee <= GRAPHENE_100_PERCENT );
      FC_ASSERT( lifetime_referrer_percent_of_fee <= GRAPHENE_100_PERCENT );
      FC_ASSERT( network_percent_of_fee + lifetime_referrer_percent_of_fee <= GRAPHENE_100_PERCENT );

      FC_ASSERT( block_interval >= GRAPHENE_MIN_BLOCK_INTERVAL );
      FC_ASSERT( block_interval <= GRAPHENE_MAX_BLOCK_INTERVAL );
      FC_ASSERT( block_interval > 0 );
      FC_ASSERT( maintenance_interval > block_interval,
                 "Maintenance interval must be longer than block interval" );
      FC_ASSERT( maintenance_interval % block_interval == 0,
                 "Maintenance interval must be a multiple of block interval" );
      FC_ASSERT( maximum_transaction_size >= GRAPHENE_MIN_TRANSACTION_SIZE_LIMIT,
                 "Transaction size limit is too low" );
      FC_ASSERT( maximum_block_size >= GRAPHENE_MIN_BLOCK_SIZE_LIMIT,
                 "Block size limit is too low" );
      FC_ASSERT( maximum_time_until_expiration > block_interval,
                 "Maximum transaction expiration time must be greater than a block interval" );
      FC_ASSERT( maximum_proposal_lifetime - committee_proposal_review_period > block_interval,
                 "Committee proposal review period must be less than the maximum proposal lifetime" );
   }

} } // graphene::chain
