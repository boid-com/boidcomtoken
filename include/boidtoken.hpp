#pragma once

#include <cmath>
#include <set>
#include <string>

#include <eosio/action.hpp>
#include <eosio/asset.hpp>
#include <eosio/contract.hpp>
#include <eosio/dispatcher.hpp>
#include <eosio/eosio.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/name.hpp>
#include <eosio/symbol.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <eosio/check.hpp>


#define STAKE_ADD 0
#define STAKE_SUB 1
#define STAKE_SEND 2
#define STAKE_RETURN 3
#define STAKE_TRANSFER 4

#define STAKE_BREAK_OFF 0  // Can not stake
#define STAKE_BREAK_ON 1   // Can stake

#define TIME_MULT 1  // seconds
// #define TIME_MULT 86400 // seconds
#define MICROSEC_MULT 1e6
#define DAY_MICROSEC 86400e6
// #define DAY_MICROSEC 6e7
// #define TIME_CAP 3600000000
#define TIME_CAP (DAY_MICROSEC * 7)  // cap claim rewards by one week

using namespace eosio;
using std::string;
using eosio::check;
using eosio::const_mem_fun;
using eosio::microseconds;
using eosio::time_point;

CONTRACT boidtoken : public contract {
 public:
  using contract::contract;

  ACTION create(name issuer, asset maximum_supply);

  ACTION issue(name to, asset quantity, string memo);

  ACTION recycle(name account, asset quantity);

  ACTION open(const name &owner, const symbol &symbol, const name &ram_payer);

  ACTION close(const name &owner, const symbol &symbol);

  ACTION transfer(name from, name to, asset quantity, string memo);

  ACTION stake(name from, name to, asset quantity, uint32_t time_limit, bool use_staked_balance);

  ACTION sendmessage(name acct, string memo);

  ACTION claim(name stake_account, float percentage_to_stake, bool issuer_claim);

  ACTION unstake(name from, name to, asset quantity,uint32_t time_limit,
  bool to_staked_balance,
  bool issuer_unstake,
  bool transfer );

  ACTION initstats(bool wpf_reset);

  ACTION updatepower(const name acct, const float boidpower);

  ACTION setpower(const name acct, const float boidpower, const bool reset_claim_time);

  ACTION matchtotdel(const name account, const asset quantity, bool subtract);

  ACTION synctotdel(const name account);

  ACTION setstakediff(const float stake_difficulty);

  ACTION setpowerdiff(const float power_difficulty);

  ACTION setpowerrate(const float power_bonus_max_rate);

  ACTION setpwrstkmul(const float powered_stake_multiplier);

  ACTION setminstake(const asset min_stake);

  ACTION setmaxpwrstk(const float percentage);

  ACTION setmaxwpfpay(const asset max_wpf_payout);

  ACTION setwpfproxy(const name wpf_proxy);

  ACTION setbpdecay(const float decay);

  ACTION setbpmult(const float update_exp);

  ACTION setbpconst(const float const_decay);

  ACTION resetbonus(const name account);

  ACTION resetpowtm(const name account, bool bonus);

  ACTION settotactive(const uint32_t active_accounts);

  ACTION settotstaked(const asset total_staked);
  ACTION closepwr(const name acct, const bool admin_auth);
  
  
  // Temp Dev Actions

  ACTION clearstakes(uint32_t rows);
  ACTION clearpwrs(uint32_t rows);
  ACTION reclaim(name acct, asset quantity);



  inline float update_boidpower(float bpPrev, float bpNew, float dt);

 private:

  TABLE stakeconfig {
    uint64_t config_id; /**< Configuration id for indexing */
    uint8_t stakebreak; /**< Activate stake break period */
    asset bonus;        /**< Stake bonus type */
    microseconds season_start;
    asset total_season_bonus;

    // bookkeeping:
    uint32_t active_accounts; /**< Total active staking accounts */
    asset total_staked;       /**< Total quantity staked */
    asset last_total_powered_stake;
    float total_boidpower;

    // staking reward equation vars:
    float stake_difficulty;
    float powered_stake_multiplier;

    float power_difficulty;
    float power_bonus_max_rate;
    asset min_stake; /**< Min staked to receive bonus */
    float max_powered_stake_ratio;
    asset max_wpf_payout;
    asset worker_proposal_fund;
    name worker_proposal_fund_proxy;

    float boidpower_decay_rate;
    float boidpower_update_mult;
    float boidpower_const_decay;

    uint64_t primary_key() const { return config_id; }  //!< Index by config id
  };

  typedef eosio::multi_index<"stakeconfigs"_n, stakeconfig> config_t;

  TABLE account {
    asset balance;

    uint64_t primary_key() const { return balance.symbol.code().raw(); }
  };

  typedef eosio::multi_index<"accounts"_n, account> accounts;

  TABLE boidpower {
    name acct;
    float quantity;

    uint64_t primary_key() const { return acct.value; }

    EOSLIB_SERIALIZE(boidpower, (acct)(quantity));
  };

  typedef eosio::multi_index<"boidpowers"_n, boidpower> boidpowers;

  TABLE power {
    name acct;
    float quantity;
    asset total_power_bonus;
    asset total_stake_bonus;
    microseconds prev_claim_time;
    microseconds prev_bp_update_time;
    asset total_delegated;

    uint64_t primary_key() const { return acct.value; }
  };

  typedef eosio::multi_index<"powers"_n, power> power_t;

  // DEPRECATED
  TABLE stakerow {
    name stake_account;
    asset staked;
    uint8_t auto_stake;  // toggle if we want to unstake stake_account at end of season

    uint64_t primary_key() const { return stake_account.value; }

    EOSLIB_SERIALIZE(stakerow, (stake_account)(staked)(auto_stake));
  };

  typedef eosio::multi_index<"stakes"_n, stakerow> staketable;

  /*!
    stake table
   */
  TABLE stake_row {
    name from;
    name to;
    asset quantity;
    asset my_bonus;  // TODO
    microseconds expiration;
    microseconds prev_claim_time;
    asset trans_quantity;
    microseconds trans_expiration;
    microseconds trans_prev_claim_time;

    uint64_t primary_key() const { return from.value; }
  };

  typedef eosio::multi_index<"staked"_n, stake_row> stake_t;

  TABLE delegations {
    name from;
    name to;
    asset quantity;
    microseconds expiration;
    asset trans_quantity;
    microseconds trans_expiration;

    uint64_t primary_key() const { return to.value; }
  };

  typedef eosio::multi_index<"delegation"_n, delegations> delegation_t;

  /*!
    Table for storing token information
   */
  TABLE currency_stat {
    asset supply;     /**< current number of BOID tokens */
    asset max_supply; /**< max number of BOID tokens */
    name issuer;      /**< name of the account that issues BOID tokens */

    // table (database) key to get read and write access
    uint64_t primary_key() const { return supply.symbol.code().raw(); }  //!< Index by token type
  };

  typedef eosio::multi_index<"stat"_n, currency_stat> stats;

  /**
    @brief subtract available balance from account
    @param owner - account to subtract tokens from
    @param value - amount to subtract
   */
  void sub_balance(name owner, asset value, name ram_payer);

  /**
    @brief add available balance to account
    @param owner - account to add tokens to
    @param value - amount to add
   */
  void add_balance(name owner, asset value, name ram_payer);

  void sub_stake(name from, name to, asset quantity, name ram_payer);

  void add_stake(name from, name to, asset quantity, microseconds expiration, name ram_payer);

  void claim_for_stake(asset staked, asset powered_staked, microseconds prev_claim_time, microseconds expiration, asset * stake_payout, asset * wpf_payout);

  void get_stake_bonus(microseconds start_time, microseconds claim_time, asset staked, asset powered_staked, asset * stake_payout, asset * wpf_payout);

  void get_power_bonus(microseconds start_time, microseconds claim_time, float boidpower, asset *power_payout);

  void add_total_delegated(name account, asset quantity, name ram_payer);

  void sub_total_delegated(name account, asset quantity, name ram_payer);

  asset get_total_delegated(name account, bool iterate);

  void sync_total_delegated(name account, name ram_payer);

  asset get_balance(name account);

  void validQuantity(asset quantity);
};

template <typename T>
void cleanTable(name code, uint64_t account, const uint32_t batchSize) {
  T db(code, account);
  uint32_t counter = 0;
  auto itr = db.begin();
  while (itr != db.end() && counter++ < batchSize) {
    itr = db.erase(itr);
  }
}

float boidtoken::update_boidpower(float bpPrev, float bpNew, float dt) {
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  // return bpNew;
  float dtReal = dt * TIME_MULT;

  print("Existing Power: ", bpPrev, "\n");
  print("Adding Power: ", bpNew, "\n");
  print("Time Difference: ", dtReal, "\n");

  float decay = pow(1.0 - c_itr->boidpower_decay_rate, dtReal);
  float constDecay = DAY_MICROSEC * TIME_MULT * c_itr->boidpower_const_decay;
  float quantity = bpPrev * decay - dtReal / constDecay;
  float updatedPower = fmax(fmax(quantity, 0) + bpNew, 0);

  if (updatedPower < 1) updatedPower = 0;

  print("update_boidpower", "\n");
  print("decay:", decay, "\n");
  print("constDecay:", constDecay, "\n");
  print("quantity:", quantity, "\n");
  print("updatedPower:", updatedPower, "\n");

  return updatedPower;
}
