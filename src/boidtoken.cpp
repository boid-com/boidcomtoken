#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <boidtoken.hpp>

ACTION boidtoken::create(name issuer, asset maximum_supply) {
  require_auth(get_self());
  auto sym = maximum_supply.symbol;
  check(sym.is_valid(), "invalid symbol name");
  check(maximum_supply.is_valid(), "invalid supply");
  check(maximum_supply.amount > 0, "max-supply must be positive");

  stats statstable(get_self(), sym.code().raw());
  auto existing = statstable.find(sym.code().raw());
  check(existing == statstable.end(), "symbol already exists");

  statstable.emplace(get_self(), [&](auto &s) {
    s.supply.symbol = maximum_supply.symbol;
    s.max_supply = maximum_supply;
    s.issuer = issuer;
  });
};

ACTION boidtoken::issue(name to, asset quantity, string memo) {
  print("issue\n");
  auto sym = quantity.symbol;
  check(sym.is_valid(), "invalid symbol name");
  check(memo.size() <= 256, "memo has more than 256 bytes");

  stats statstable(get_self(), sym.code().raw());
  auto existing = statstable.find(sym.code().raw());
  check(existing != statstable.end(), "symbol does not exist, create token with symbol before issuing said token");
  const auto &st = *existing;

  require_auth(st.issuer);
  validQuantity(quantity);

  check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

  check(quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

  statstable.modify(st, _self, [&](auto &s) { s.supply += quantity; });

  add_balance(st.issuer, quantity, st.issuer);

  if (to != st.issuer) {
    action(permission_level{st.issuer, "active"_n}, get_self(), "transfer"_n, std::make_tuple(st.issuer, to, quantity, memo)).send();
  }
};

ACTION boidtoken::recycle(name account, asset quantity) {
  require_auth(account);
  print("recycle\n");
  validQuantity(quantity);
  auto sym = quantity.symbol;
  stats statstable(_self, sym.code().raw());
  auto existing = statstable.find(sym.code().raw());
  check(existing != statstable.end(), "symbol does not exist in stats table");
  const auto &st = *existing;

  check(sym == st.supply.symbol, "symbol precision mismatch");

  power_t p_t(get_self(), account.value);
  auto p_itr = p_t.find(account.value);
  if (p_itr == p_t.end() || p_itr->total_delegated.symbol != sym) sync_total_delegated(account, account);

  asset available = get_balance(account) - get_total_delegated(account, false);
  check(quantity <= available, "transferring more than available balance");

  sub_balance(account, quantity, account);

  statstable.modify(st, _self, [&](auto &s) {
    s.supply -= quantity;

    if (s.supply.amount < 0) {
      print("Warning: recycle sets supply below 0. Please check this out. Setting supply to 0");
      s.supply = asset{0, quantity.symbol};
    }
  });
};

ACTION boidtoken::open(const name &owner, const symbol &symbol, const name &ram_payer) {
  require_auth(ram_payer);

  check(is_account(owner), "owner account does not exist");

  auto sym_code_raw = symbol.code().raw();
  stats statstable(get_self(), sym_code_raw);
  const auto &st = statstable.get(sym_code_raw, "symbol does not exist");
  check(st.supply.symbol == symbol, "symbol precision mismatch");

  accounts acnts(get_self(), owner.value);
  auto it = acnts.find(sym_code_raw);
  if (it == acnts.end()) {
    acnts.emplace(ram_payer, [&](auto &a) { a.balance = asset{0, symbol}; });
  } else if (owner == ram_payer) {
    acnts.modify(it, ram_payer, [&](auto &a) {});
  }
};

ACTION boidtoken::close(const name &owner, const symbol &symbol) {
  require_auth(owner);
  accounts acnts(get_self(), owner.value);
  auto it = acnts.find(symbol.code().raw());
  check(it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect.");
  check(it->balance.amount == 0, "Cannot close because the balance is not zero.");
  acnts.erase(it);
};

ACTION boidtoken::transfer(name from, name to, asset quantity, string memo) {
  print("transfer\n");
  check(from != to, "cannot transfer to self");
  require_auth(from);
  check(is_account(to), "to account does not exist");
  auto sym = quantity.symbol;
  stats statstable(_self, sym.code().raw());
  const auto &st = statstable.get(sym.code().raw());
  validQuantity(quantity);

  require_recipient(from);
  require_recipient(to);

  check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");
  check(memo.size() <= 256, "memo has more than 256 bytes");

  power_t p_from_t(get_self(), from.value), p_to_t(get_self(), to.value);
  auto p_from_itr = p_from_t.find(from.value), p_to_itr = p_to_t.find(to.value);

  if (p_from_itr == p_from_t.end() || p_from_itr->total_delegated.symbol != sym) sync_total_delegated(from, from);
  if (from != to && (p_to_itr == p_to_t.end() || p_to_itr->total_delegated.symbol != sym)) sync_total_delegated(to, from);

  asset available = get_balance(from) - get_total_delegated(from, false);

  check(quantity <= available, "Transfer quantity exceeds liquid balance.");

  sub_balance(from, quantity, same_payer);
  add_balance(to, quantity, from);
};

ACTION boidtoken::stake(name from, name to, asset quantity, uint32_t time_limit) {
  require_auth(from);

  config_t c_t(get_self(), get_self().value);

  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");

  check(is_account(from), "From account does not exist.");
  check(is_account(to), "To account does not exist.");

  auto sym = quantity.symbol;
  stats statstable(_self, sym.code().raw());
  validQuantity(quantity);

  const auto &st = statstable.get(sym.code().raw());
  check(quantity.symbol == st.supply.symbol, "symbol precision mismatch");

  power_t p_from_t(get_self(), from.value), p_to_t(get_self(), to.value);
  auto p_from_itr = p_from_t.find(from.value), p_to_itr = p_to_t.find(to.value);

  if (p_from_itr == p_from_t.end() || p_from_itr->total_delegated.symbol != sym) sync_total_delegated(from, from);
  if (from != to && (p_to_itr == p_to_t.end() || p_to_itr->total_delegated.symbol != sym)) sync_total_delegated(to, from);

  delegation_t deleg_t(get_self(), from.value);
  auto deleg = deleg_t.find(from.value);
  asset available = asset{0, sym};

  available = get_balance(from) - get_total_delegated(from, false);

  check(quantity <= available, "Stake quantity exceeds liquid balance.");

  stake_t s_t(get_self(), to.value);

  time_point t = current_time_point();
  microseconds curr_time = t.time_since_epoch(), expiration_time = time_limit == 0 ? microseconds(0) : curr_time + microseconds(time_limit * MICROSEC_MULT), self_expiration_time;

  auto s_itr = s_t.find(from.value);
  if (s_itr != s_t.end()) check(expiration_time >= s_itr->expiration, "Must specify a later expiration time than the existing stake.");

  string memo, account_type;

  accounts accts(get_self(), from.value);
  const auto &acct = accts.get(quantity.symbol.code().raw(), "No account object found.");
  check(quantity <= acct.balance, "Staking more than available liquid balance.");

  add_stake(from, to, quantity, expiration_time, from);

  account_type = "liquid";

  memo = "account:  " + from.to_string() + " using " + account_type +
         " balance"
         "\naction: stake" +
         "\ndelegate: " + to.to_string() + " " + "\namount: " + quantity.to_string() + " " + "\ntimeout " + std::to_string(time_limit) + " seconds";

  action(permission_level{from, "active"_n}, get_self(), "sendmessage"_n, std::make_tuple(from, memo)).send();
};

ACTION boidtoken::sendmessage(name acct, string memo) {
  require_auth(acct);
  check(memo.size() <= 256, "memo has more than 256 bytes");
  print(memo);
};

ACTION boidtoken::clearstakes(name scope) {
  require_auth(get_self());
  cleanTable<staketable>(get_self(), scope.value, 20);
};

ACTION boidtoken::clearpwrs(name scope) {
  require_auth(get_self());
  cleanTable<boidpowers>(get_self(), scope.value, 20);
};

ACTION boidtoken::claim(name stake_account, bool issuer_claim) {
  print("claim \n");
  config_t c_t(get_self(), get_self().value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");

  auto sym = c_itr->bonus.symbol;
  float precision_coef = pow(10, sym.precision());

  accounts accts(get_self(), stake_account.value);
  const auto &a_itr = accts.find(sym.code().raw());
  check(a_itr != accts.end(), "Account must have a BOID balance or use the \"open\" action before calling claim.");

  stats statstable(get_self(), sym.code().raw());
  auto existing = statstable.find(sym.code().raw());
  check(existing != statstable.end(), "This symbol does not exist in this contract.");
  const auto &st = *existing;

  if (issuer_claim)
    require_auth(st.issuer);
  else
    require_auth(stake_account);

  asset zerotokens = asset{0, sym}, total_payout = zerotokens, power_payout = zerotokens, stake_payout = zerotokens, wpf_payout = zerotokens, powered_stake = zerotokens, expired_received_tokens = zerotokens, expired_delegated_tokens = zerotokens;

  name ram_payer = issuer_claim ? st.issuer : stake_account;

  power_t pow_t(get_self(), stake_account.value);
  auto bp = pow_t.find(stake_account.value);

  if (bp == pow_t.end() || bp->total_delegated.symbol != sym) sync_total_delegated(stake_account, ram_payer);

  bp = pow_t.find(stake_account.value);
  print("Existing Boid Power:", bp->quantity, "\n");

  time_point t = current_time_point();

  microseconds curr_time = t.time_since_epoch(), start_time = microseconds(0), claim_time = microseconds(0), self_expiration = microseconds(0), zeroseconds = microseconds(0);

  float boidpower = update_boidpower(bp->quantity, 0, (curr_time - bp->prev_bp_update_time).count());
  print("New Boid Power:", boidpower, "\n");

  stake_t s_t(get_self(), stake_account.value);
  auto self_stake = s_t.find(stake_account.value);

  if (self_stake == s_t.end())
    self_expiration = microseconds(0);
  else
    self_expiration = self_stake->expiration;

  // Find stake bonus
  float powered_stake_amount = fmin(c_itr->powered_stake_multiplier * boidpower * precision_coef, c_itr->max_powered_stake_ratio * c_itr->total_staked.amount);

  print("min: ", c_itr->max_powered_stake_ratio * c_itr->total_staked.amount, "\n");
  print("powered stake amount: ", powered_stake_amount, "\n");
  powered_stake = asset{(int64_t)powered_stake_amount, sym};

  asset curr_stake_payout = zerotokens, curr_wpf_payout = zerotokens;
  for (auto it = s_t.begin(); it != s_t.end(); it++) {
    if (it->quantity.amount > 0) {
      claim_for_stake(it->quantity, powered_stake, it->prev_claim_time, it->expiration, &curr_stake_payout, &curr_wpf_payout);

      stake_payout += curr_stake_payout;
      wpf_payout += curr_wpf_payout;

      powered_stake -= it->quantity;
      powered_stake = powered_stake < zerotokens ? zerotokens : powered_stake;

      stake_t from_stake(get_self(), it->from.value);
      auto from_self_stake = from_stake.find(it->from.value);
      microseconds from_self_expiration, from_self_trans_expiration;

      if (from_self_stake == from_stake.end())
        from_self_expiration = microseconds(0);
      else
        from_self_expiration = from_self_stake->expiration;

      s_t.modify(it, same_payer, [&](auto &s) { s.prev_claim_time = curr_time; });
    }
  }

  wpf_payout = wpf_payout < c_itr->max_wpf_payout ? wpf_payout : c_itr->max_wpf_payout;
  total_payout += stake_payout + wpf_payout;

  // Find power bonus and update power and claim parameters
  if (a_itr != accts.end()) {
    start_time = bp->prev_claim_time;
    claim_time = curr_time;
    get_power_bonus(start_time, claim_time, boidpower, &power_payout);

    total_payout += power_payout;

    string debugStr = "Payout would cause token supply to exceed maximum\nstake account: " + stake_account.to_string() + "\ntotal payout: " + total_payout.to_string() + "\npower payout: " + power_payout.to_string() + "\nstake payout: " + stake_payout.to_string();

    check(total_payout <= existing->max_supply - existing->supply, debugStr);

    check(total_payout.amount >= 0 && power_payout.amount >= 0 && stake_payout.amount >= 0 && wpf_payout.amount >= 0, "All payouts must be zero or positive quantities");

    asset self_payout = power_payout + stake_payout;

    string memo = "account:  " + stake_account.to_string() + "\naction: claim" + "\nstake bonus: " + stake_payout.to_string() + "\npower bonus: " + power_payout.to_string() + "\nwpf contribution: " + wpf_payout.to_string() + "\nreturning " + expired_received_tokens.to_string() + " expired tokens" + "\nreceiving " + expired_delegated_tokens.to_string() + " delegated tokens";

    if (self_payout.amount != 0) {
      action(permission_level{st.issuer, "active"_n}, get_self(), "issue"_n, std::make_tuple(stake_account, self_payout, memo)).send();
    }

    if (ram_payer == st.issuer) ram_payer = same_payer;
    pow_t.modify(bp, ram_payer, [&](auto &p) {
      p.prev_claim_time = curr_time;
      p.total_power_bonus += power_payout;
      p.total_stake_bonus += stake_payout;
      p.quantity = boidpower;
      p.prev_bp_update_time = curr_time;
    });

    if (wpf_payout != zerotokens) {
      action(permission_level{st.issuer, "active"_n}, get_self(), "issue"_n, std::make_tuple(c_itr->worker_proposal_fund_proxy, wpf_payout, memo)).send();
    }
  }
}

ACTION boidtoken::unstake(name from, name to, asset quantity) {
  config_t c_t(get_self(), get_self().value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");

  auto sym = quantity.symbol;

  stats statstable(get_self(), sym.code().raw());
  const auto &st = statstable.get(sym.code().raw());
  check(sym == st.supply.symbol, "symbol precision mismatch");

  power_t p_from_t(get_self(), from.value), p_to_t(get_self(), to.value);

  auto p_from_itr = p_from_t.find(from.value), p_to_itr = p_to_t.find(to.value);

  require_auth(from);

  if (p_from_itr == p_from_t.end() || p_from_itr->total_delegated.symbol != sym) sync_total_delegated(from, from);
  if (from != to && (p_to_itr == p_to_t.end() || p_to_itr->total_delegated.symbol != sym)) sync_total_delegated(to, from);

  stake_t s_t(get_self(), to.value);
  auto s_itr = s_t.find(from.value);
  check(s_itr != s_t.end(), "Nothing to unstake");
  validQuantity(quantity);

  asset previous_stake_amount = s_itr->quantity;
  asset amount_after = previous_stake_amount - quantity;

  check(amount_after.amount >= 0, "After unstake, must have nothing staked or a valid amount");

  time_point t = current_time_point();
  microseconds curr_expiration_time = s_itr->expiration, curr_time = t.time_since_epoch();
  print("curr time: ", curr_time.count());
  print("expiration time: ", curr_expiration_time.count());
  check(curr_expiration_time < curr_time, "Account can't unstake before timed stake has expired.");

  if (curr_expiration_time != microseconds(0)) check(previous_stake_amount == quantity, "Must unstake all tokens for definite-time stake");

  sub_stake(from, to, quantity, from);
}

ACTION boidtoken::initstats(bool wpf_reset) {
  print("initstats\n");
  require_auth(get_self());

  config_t c_t(get_self(), get_self().value);
  auto c_itr = c_t.find(0);
  auto sym = symbol("BOID", 4);
  asset cleartokens = asset{0, sym};

  // verify symbol
  stats statstable(get_self(), sym.code().raw());
  const auto &st = statstable.get(sym.code().raw());
  check(sym == st.supply.symbol, "symbol precision mismatch");
  require_auth(st.issuer);

  float precision_coef = pow(10, sym.precision());
  if (c_itr == c_t.end()) {
    c_t.emplace(get_self(), [&](auto &c) {
      c.config_id = 0;
      c.stakebreak = STAKE_BREAK_ON;

      c.total_season_bonus = cleartokens;

      c.bonus = cleartokens;
      c.total_staked = cleartokens;
      c.active_accounts = 0;

      int64_t min_stake = (int64_t)precision_coef * 1e5;
      c.min_stake = asset{min_stake, sym};
      c.power_difficulty = 25e9;
      c.stake_difficulty = 11e10;
      c.powered_stake_multiplier = 100;
      c.power_bonus_max_rate = 5.8e-7;
      c.max_powered_stake_ratio = .05;

      c.max_wpf_payout = asset{(int64_t)(10000 * precision_coef), sym};
      c.worker_proposal_fund = cleartokens;
      c.boidpower_decay_rate = 9.9e-8;
      c.boidpower_update_mult = 0.05;
      c.boidpower_const_decay = 100;
    });
  } else {
    c_t.modify(c_itr, get_self(), [&](auto &c) {
      c.stakebreak = STAKE_BREAK_ON;

      c.total_season_bonus = cleartokens;

      c.bonus = cleartokens;
      c.total_staked = cleartokens;
      c.active_accounts = 0;

      int64_t min_stake = (int64_t)precision_coef * 1e5;
      c.min_stake = asset{min_stake, sym};
      c.power_difficulty = 25e9;
      c.stake_difficulty = 11e10;
      c.powered_stake_multiplier = 100;
      c.power_bonus_max_rate = 5.8e-7;
      c.max_powered_stake_ratio = .05;

      c.max_wpf_payout = asset{(int64_t)(10000 * precision_coef), sym};
      if (wpf_reset) {
        c.worker_proposal_fund = cleartokens;
      }
      c.boidpower_decay_rate = 9.9e-8;
      c.boidpower_update_mult = 0.05;
      c.boidpower_const_decay = 100;
    });
  }
}

ACTION boidtoken::updatepower(const name acct, const float boidpower) {
  check(is_account(acct), "Account does not exist.");
  check(boidpower >= 0, "Power updates must be higher than zero.");
  require_auth(get_self());

  power_t p_t(get_self(), acct.value);

  time_point t = current_time_point();
  microseconds curr_time = t.time_since_epoch(), zeroseconds = microseconds(0);

  auto sym = symbol("BOID", 4);
  asset zerotokens = asset{0, sym};

  auto bp = p_t.find(acct.value);
  if (bp == p_t.end()) {
    p_t.emplace(get_self(), [&](auto &p) {
      p.acct = acct;
      p.prev_bp_update_time = curr_time;
      p.quantity = update_boidpower(0, boidpower, 0);
      p.total_power_bonus = zerotokens;
      p.total_stake_bonus = zerotokens;
      p.prev_claim_time = curr_time;
      p.total_delegated = get_total_delegated(acct, true);
    });
  } else {
    p_t.modify(bp, same_payer, [&](auto &p) {
      p.quantity = update_boidpower(p.quantity, boidpower, (curr_time - p.prev_bp_update_time).count());
      p.prev_bp_update_time = curr_time;
      if (p.total_delegated.symbol != sym) p.total_delegated = get_total_delegated(acct, true);
    });
  }
}

ACTION boidtoken::setpower(const name acct, const float boidpower, const bool reset_claim_time) {
  require_auth(get_self());
  check(is_account(acct), "Account does not exist.");
  check(boidpower >= 0, "Can only have zero or positive boidpower.");
  power_t p_t(get_self(), acct.value);
  time_point t = current_time_point();
  microseconds curr_time = t.time_since_epoch(), zeroseconds = microseconds(0);

  auto sym = symbol("BOID", 4);
  asset zerotokens = asset{0, sym};

  auto bp = p_t.find(acct.value);
  if (bp == p_t.end()) {
    p_t.emplace(get_self(), [&](auto &p) {
      p.acct = acct;
      p.prev_bp_update_time = curr_time;
      p.quantity = boidpower;
      p.total_power_bonus = zerotokens;
      p.total_stake_bonus = zerotokens;
      p.prev_claim_time = curr_time;
      p.total_delegated = get_total_delegated(acct, true);
    });
  } else {
    p_t.modify(bp, same_payer, [&](auto &p) {
      p.quantity = boidpower;
      p.prev_bp_update_time = curr_time;
      if (reset_claim_time) p.prev_claim_time = curr_time;
      if (p.total_delegated.symbol != sym) p.total_delegated = get_total_delegated(acct, true);
    });
  }
}

ACTION boidtoken::matchtotdel(const name account, const asset quantity, bool subtract) {
  require_auth(get_self());
  if (subtract) {
    sub_total_delegated(account, quantity, get_self());
  } else {
    add_total_delegated(account, quantity, get_self());
  }
}

ACTION boidtoken::synctotdel(const name account) {
  require_auth(get_self());
  sync_total_delegated(account, get_self());
}

ACTION boidtoken::setstakediff(const float stake_difficulty) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.stake_difficulty = stake_difficulty; });
}

ACTION boidtoken::setpowerdiff(const float power_difficulty) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.power_difficulty = power_difficulty; });
}

ACTION boidtoken::setpowerrate(const float power_bonus_max_rate) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.power_bonus_max_rate = power_bonus_max_rate; });
}

ACTION boidtoken::setpwrstkmul(const float powered_stake_multiplier) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.powered_stake_multiplier = powered_stake_multiplier; });
}

ACTION boidtoken::setminstake(const asset min_stake) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.min_stake = min_stake; });
}

ACTION boidtoken::setmaxpwrstk(const float percentage) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.max_powered_stake_ratio = percentage / 100; });
}

ACTION boidtoken::setmaxwpfpay(const asset max_wpf_payout) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.max_wpf_payout = max_wpf_payout; });
}

ACTION boidtoken::setwpfproxy(const name wpf_proxy) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.worker_proposal_fund_proxy = wpf_proxy; });
}

ACTION boidtoken::collectwpf() {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  add_balance(c_itr->worker_proposal_fund_proxy, c_itr->worker_proposal_fund, get_self());
  c_t.modify(c_itr, _self, [&](auto &c) { c.worker_proposal_fund -= c.worker_proposal_fund; });
}

ACTION boidtoken::recyclewpf() {
  require_auth(get_self());

  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");

  symbol sym = symbol("BOID", 4);
  stats statstable(_self, sym.code().raw());
  auto existing = statstable.find(sym.code().raw());
  check(existing != statstable.end(), "symbol does not exist in stats table");
  const auto &st = *existing;

  require_auth(st.issuer);

  statstable.modify(st, _self, [&](auto &s) {
    s.supply -= c_itr->worker_proposal_fund;
    if (s.supply.amount < 0) {
      print("Warning: recycle sets   supply below 0. Please check this out. Setting supply to 0");
      s.supply = asset{0, sym};
    }
  });
  c_t.modify(c_itr, _self, [&](auto &c) { c.worker_proposal_fund -= c.worker_proposal_fund; });
}

ACTION boidtoken::setbpdecay(const float decay) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.boidpower_decay_rate = decay; });
}

ACTION boidtoken::setbpmult(const float update_mult) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.boidpower_update_mult = update_mult; });
}

ACTION boidtoken::setbpconst(const float const_decay) {
  require_auth(get_self());
  config_t c_t(_self, _self.value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");
  c_t.modify(c_itr, _self, [&](auto &c) { c.boidpower_const_decay = const_decay; });
}

ACTION boidtoken::resetbonus(const name account) {
  require_auth(get_self());
  symbol sym = symbol("BOID", 4);
  power_t p_t(_self, account.value);
  auto p_itr = p_t.find(account.value);
  if (p_itr != p_t.end()) {
    p_t.modify(p_itr, same_payer, [&](auto &p) {
      p.total_power_bonus = asset{0, sym};
      p.total_stake_bonus = asset{0, sym};
    });
  }
}

ACTION boidtoken::resetpowtm(const name account, bool current) {
  require_auth(get_self());
  power_t p_t(_self, account.value);
  auto p_itr = p_t.find(account.value);
  if (p_itr != p_t.end()) {
    p_t.modify(p_itr, same_payer, [&](auto &p) {
      p.prev_claim_time = microseconds(0);
      p.prev_bp_update_time = microseconds(0);
    });
  }
}

void boidtoken::sub_balance(name owner, asset value, name ram_payer) {
  accounts from_acnts(get_self(), owner.value);
  const auto &from = from_acnts.get(value.symbol.code().raw(), "no balance object found");
  check(from.balance.amount >= value.amount, "overdrawn balance");

  from_acnts.modify(from, ram_payer, [&](auto &a) { a.balance -= value; });
}

void boidtoken::add_balance(name owner, asset value, name ram_payer) {
  accounts to_acnts(get_self(), owner.value);
  auto to = to_acnts.find(value.symbol.code().raw());

  if (to == to_acnts.end()) {
    to_acnts.emplace(ram_payer, [&](auto &a) { a.balance = value; });
  } else {
    to_acnts.modify(to, same_payer, [&](auto &a) { a.balance += value; });
  }
}

void boidtoken::sub_stake(name from, name to, asset quantity, name ram_payer) {
  delegation_t deleg_t(get_self(), from.value);
  auto deleg = deleg_t.find(to.value);
  check(deleg != deleg_t.end(), "Delegation does not exist");

  stake_t s_t(get_self(), to.value);
  auto to_itr = s_t.find(from.value);
  check(to_itr != s_t.end(), "Stake does not exist");

  asset curr_delegated_to = to_itr->quantity;
  asset after_delegated_to = curr_delegated_to - quantity;

  config_t c_t(get_self(), get_self().value);
  auto c_itr = c_t.find(0);
  check(after_delegated_to.amount >= 0, "final delegation should be 0 or greater");

  sub_total_delegated(from, quantity, from);

  deleg_t.modify(deleg, ram_payer, [&](auto &d) { d.quantity = after_delegated_to; });
  s_t.modify(to_itr, ram_payer, [&](auto &s) { s.quantity = after_delegated_to; });
  c_t.modify(c_itr, get_self(), [&](auto &c) {
    if (after_delegated_to.amount == 0) c.active_accounts -= 1;
    c.total_staked -= quantity;
  });
}

void boidtoken::add_stake(name from, name to, asset quantity, microseconds expiration, name ram_payer) {
  symbol sym = quantity.symbol;
  asset zerotokens = asset{0, sym};

  stake_t s_t(get_self(), to.value);
  auto to_itr = s_t.find(from.value);

  delegation_t deleg_t(get_self(), from.value);
  auto deleg = deleg_t.find(to.value);

  config_t c_t(get_self(), get_self().value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");

  time_point t = current_time_point();
  microseconds curr_time = t.time_since_epoch();

  if (deleg == deleg_t.end()) {
    deleg_t.emplace(ram_payer, [&](auto &a) {
      a.from = from;
      a.to = to;
      a.quantity = quantity;
      a.expiration = expiration;
      a.trans_quantity = zerotokens;
    });
    s_t.emplace(ram_payer, [&](auto &s) {
      s.from = from;
      s.to = to;
      s.my_bonus = zerotokens;
      s.quantity = quantity;
      s.expiration = expiration;
      s.prev_claim_time = microseconds(0);
      s.trans_quantity = zerotokens;
      s.prev_claim_time = curr_time;
    });
    c_t.modify(c_itr, get_self(), [&](auto &c) { c.active_accounts += 1; });
  } else {
    check(to_itr != s_t.end(), "Entry exists in delegation table but not stake table");
    deleg_t.modify(deleg, same_payer, [&](auto &a) {
      a.quantity += quantity;
      a.expiration = expiration;
    });
    s_t.modify(to_itr, same_payer, [&](auto &s) {
      s.quantity += quantity;
      s.expiration = expiration;
    });
  }
  c_t.modify(c_itr, _self, [&](auto &c) { c.total_staked += quantity; });

  add_total_delegated(from, quantity, ram_payer);
}

void boidtoken::claim_for_stake(asset staked, asset powered_staked, microseconds prev_claim_time, microseconds expiration, asset *stake_payout, asset *wpf_payout) {
  symbol sym = staked.symbol;
  asset zerotokens = asset{0, sym};

  config_t c_t(get_self(), get_self().value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");

  time_point t = current_time_point();
  microseconds curr_time = t.time_since_epoch(), start_time, claim_time;

  if (prev_claim_time > curr_time) {
    *stake_payout = zerotokens;
    *wpf_payout = zerotokens;
  };
  start_time = prev_claim_time == microseconds(0) ? curr_time : prev_claim_time;

  claim_time = curr_time;

  check(claim_time <= curr_time, "invalid payout date");

  get_stake_bonus(start_time, claim_time, staked, powered_staked, stake_payout, wpf_payout);
}

void boidtoken::get_stake_bonus(microseconds start_time, microseconds claim_time, asset staked, asset powered_staked, asset *stake_payout, asset *wpf_payout) {
  config_t c_t(get_self(), get_self().value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");

  symbol sym = staked.symbol;
  int64_t precision_coef = pow(10, sym.precision());
  float amt = fmin((float)staked.amount, (float)powered_staked.amount);
  float wpf_amt = fmax((float)(staked.amount - powered_staked.amount), 0);
  // Maximum stake time is capped
  float timeDifference = fmin((claim_time - start_time).count(), TIME_CAP);
  float stake_coef = timeDifference * TIME_MULT / c_itr->stake_difficulty;
  int64_t payout_amount = (int64_t)(amt * stake_coef) / precision_coef;
  int64_t wpf_payout_amount = (int64_t)(wpf_amt * stake_coef) / precision_coef;
  *stake_payout = asset{payout_amount, sym};
  *wpf_payout = asset{wpf_payout_amount, sym};
  *wpf_payout = *wpf_payout > c_itr->max_wpf_payout ? c_itr->max_wpf_payout : *wpf_payout;

  print("GET STAKE BONUS\n");
  print("-\n");
  print("staked amount: ", staked.amount, "\n");
  print("powered staked amount: ", powered_staked.amount, "\n");
  print("timeDifference: ", timeDifference, "\n");
  print("stake coef: ", stake_coef, "\n");
  print("wpf amt: ", wpf_amt, "\n");
  print("wpf payout: ", wpf_payout->to_string(), "\n");
  print("stake payout: ", stake_payout->to_string(), "\n");
  print("-\n");
}

void boidtoken::get_power_bonus(microseconds start_time, microseconds claim_time, float boidpower, asset *power_payout) {
  config_t c_t(get_self(), get_self().value);
  auto c_itr = c_t.find(0);
  check(c_itr != c_t.end(), "Must first initstats");

  symbol sym = symbol("BOID", 4);
  int64_t precision_coef = pow(10, sym.precision());
  float power_coef = fmin((float)(boidpower / c_itr->power_difficulty), c_itr->power_bonus_max_rate);

  if (start_time.count() == 0) start_time = claim_time;
  float timeDifference = fmin((claim_time - start_time).count(), TIME_CAP);
  *power_payout = asset{(int64_t)(power_coef * timeDifference * TIME_MULT), sym};

  print("GET POWER BONUS\n");
  print("-\n");
  print("dt: ", (claim_time - start_time).count(), "\n");
  print("TimeDifference:", timeDifference, "\n");
  print("power coef: ", power_coef, "\n");
  print("power payout: ", power_payout->to_string(), "\n");
  print("-\n");
}

// TODO get rid of extra stuff after contract fix
void boidtoken::sub_total_delegated(name account, asset quantity, name ram_payer) {
  power_t p_t(_self, account.value);
  auto p_itr = p_t.find(account.value);
  symbol sym = symbol("BOID", 4);
  check(p_itr != p_t.end() && p_itr->total_delegated.symbol == sym, "Must first sync total delegated");

  p_t.modify(p_itr, same_payer, [&](auto &p) { p.total_delegated -= quantity; });
}

void boidtoken::add_total_delegated(name account, asset quantity, name ram_payer) {
  power_t p_t(_self, account.value);
  auto p_itr = p_t.find(account.value);
  symbol sym = symbol("BOID", 4);
  check(p_itr != p_t.end() && p_itr->total_delegated.symbol == sym, "Must first sync total delegated");
  p_t.modify(p_itr, same_payer, [&](auto &p) { p.total_delegated += quantity; });
}

asset boidtoken::get_total_delegated(name account, bool iterate) {
  symbol sym = symbol("BOID", 4);
  asset zerotokens = asset{0, sym};
  if (!iterate) {
    power_t p_t(_self, account.value);
    auto p_itr = p_t.find(account.value);
    symbol sym = symbol("BOID", 4);
    check(p_itr != p_t.end() && p_itr->total_delegated.symbol == sym, "Must first sync total delegated");
    return p_itr->total_delegated;
  } else {
    delegation_t d_t(get_self(), account.value);
    asset total_delegated = zerotokens;
    for (auto d_itr = d_t.begin(); d_itr != d_t.end(); d_itr++) {
      total_delegated += d_itr->quantity;
    }
    stake_t s_t(get_self(), account.value);
    for (auto s_itr = s_t.begin(); s_itr != s_t.end(); s_itr++) {
      total_delegated += s_itr->trans_quantity;
    }
    return total_delegated;
  }
}

void boidtoken::sync_total_delegated(name account, name ram_payer) {
  asset total_delegated = get_total_delegated(account, true);

  power_t p_t(_self, account.value);
  auto p_itr = p_t.find(account.value);

  if (p_itr != p_t.end()) {
    p_t.modify(p_itr, same_payer, [&](auto &p) { p.total_delegated = total_delegated; });
  } else {
    time_point t = current_time_point();
    microseconds curr_time = t.time_since_epoch(), zeroseconds = microseconds(0);
    asset zerotokens = asset{0, total_delegated.symbol};
    p_t.emplace(ram_payer, [&](auto &p) {
      p.acct = account;
      p.prev_bp_update_time = curr_time;
      p.quantity = 0;
      p.total_power_bonus = zerotokens;
      p.total_stake_bonus = zerotokens;
      p.prev_claim_time = curr_time;
      p.total_delegated = total_delegated;
    });
  }
}

asset boidtoken::get_balance(name account) {
  accounts accountstable(get_self(), account.value);
  symbol sym = symbol("BOID", 4);
  const auto &ac = accountstable.get(sym.code().raw());
  return ac.balance;
}

void boidtoken::validQuantity(asset quantity) {
  check(quantity.symbol.is_valid(), "invalid symbol name");
  check(quantity.is_valid(), "invalid quantity");
  check(quantity.amount > 0, "quantity must be greater than zero");
}

extern "C" {
[[noreturn]] void apply(uint64_t receiver, uint64_t code, uint64_t action) {
  if (code == receiver) {
    switch (action) {
      EOSIO_DISPATCH_HELPER(boidtoken, (create)(issue)(recycle)(clearpwrs)(clearstakes)
                            (open)(close)(transfer)(stake)(sendmessage)(claim)(unstake)(initstats)(updatepower)(setpower)(matchtotdel)(synctotdel)(setstakediff)(setpowerdiff)(setpowerrate)(setpwrstkmul)(setminstake)(setmaxpwrstk)(setmaxwpfpay)(setwpfproxy)(collectwpf)(recyclewpf)(setbpdecay)(setbpmult)(setbpconst)(resetbonus)(resetpowtm))
    }
  }
  eosio_exit(0);
}
}
