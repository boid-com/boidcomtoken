const boidjs = require('boidjs')({api:{},rpc:{}})

function setGlobals(account,config){
  const tx = boidjs.tx
  var actions = []
  const auth = {accountName:account, permission:"active"}

  if (config.min_stake) {
    const data = {min_stake:config.min_stake}
    const name = "setminstake"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  if (config.stake_difficulty) {
    const data = {stake_difficulty:config.stake_difficulty}
    const name = "setstakediff"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  if (config.powered_stake_multiplier) {
    const data = {powered_stake_multiplier:config.powered_stake_multiplier}
    const name = "setpwrstkmul"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  if (config.power_difficulty) {
    const data = {power_difficulty:config.power_difficulty}
    const name = "setpowerdiff"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  if (config.power_bonus_max_rate) {
    const data = {power_bonus_max_rate:config.power_bonus_max_rate}
    const name = "setpowerrate"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  if (config.max_powered_stake_ratio) {
    const data = {percentage:config.max_powered_stake_ratio * 100}
    const name = "setmaxpwrstk"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  if (config.boidpower_update_mult) {
    const data = {update_exp:config.boidpower_update_mult}
    const name = "setbpmult"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  if (config.boidpower_decay_rate) {
    const data = {decay:config.boidpower_decay_rate}
    const name = "setbpdecay"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  if (config.boidpower_const_decay) {
    const data = {const_decay:config.boidpower_const_decay}
    const name = "setbpconst"
    actions.push(tx.maketx({account,name,auth,data}).actions[0])
  }

  console.log(JSON.stringify(actions,null,2))
  // const result = await api.transact({actions},tx.tapos).catch(el => console.error(el.toString()))
  return actions
  if (result) console.log(result)

}

module.exports = setGlobals