const env = require('../.env.js')
const accts = require('../accounts')
const acct = (name) => accts[env.network][name]
const { api, rpc, boidjs } = require('../eosjs')(env.keys[env.network])
const tapos = { blocksBehind: 6, expireSeconds: 60 }

const targetAccount = 'boid'
const config = { issueTokens: 200, setPower: 100000 }

async function claim (targetAccount) {
  const wallet = await boidjs.get.wallet(targetAccount)
  const prediction = await boidjs.get.pendingClaim(wallet)
  console.log('Payout Prediction:', prediction)
  const contract = acct('token')
  const authorization = [{ actor: targetAccount, permission: 'active' }]
  const account = contract
  try {
    const result = await api.transact({
      actions: [
        { account, authorization, name: 'claim', data: { stake_account: targetAccount, issuer_claim: false,percentage_to_stake:0 } }
      ]
    }, tapos)
    console.log(result.transaction_id)
    const power = await boidjs.get.powerStats(targetAccount)
    console.log('current power:', parseFloat(power.quantity))

    // const transaction = await rpc.history_get_transaction(result.transaction_id)
    // console.log(JSON.stringify(transaction,null,2))
  } catch (error) {
    console.log(arguments.callee.name, error.toString())
    // return new Error(error.toString())
    // return Promise.reject(error)
  }
}

async function stake (from, to, quantity, time_limit) {
  try {
    if (!time_limit) time_limit = 0
    const authorization = [{ actor: from, permission: 'active' }]
    const account = acct('token')
    quantity = parseFloat(quantity).toFixed(4) + ' BOID'
    const actions = [{ account, authorization, name: 'stake', data: { from, to, quantity, time_limit,use_staked_balance:false } }]
    const result = await api.transact({ actions }, tapos)
    console.log(result.transaction_id)
    const powers = await boidjs.get.powerStats(from)
    console.log(powers)
  } catch (error) {
    console.log(arguments.callee.name, error.toString())

    // return Promise.reject(error.toString())
  }
}

async function unstake (from, to, quantity, time_limit, transfer) {
  try {
    transfer = (transfer == 'true')
    quantity = parseFloat(quantity).toFixed(4) + ' BOID'
    time_limit = parseFloat(time_limit)
    const contract = acct('token')
    let actor
    if (transfer) actor = to
    else actor = from
    const authorization = [{ actor, permission: 'active' }]
    const account = contract
    const result = await api.transact({
      actions: [
        {
          account,
          authorization,
          name: 'unstake',
          data: { from, to, quantity, time_limit, issuer_unstake: false, transfer, to_staked_balance:false }
        }
      ]
    }, tapos)
    console.log(result.transaction_id)
    const stakes = await boidjs.get.stakes(from)
    console.log(stakes)
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

async function transstake () {
  try {
    const contract = acct('token')
    const targetAccount = 'john'
    const toAccount = 'boid'
    const authorization = [{ actor: targetAccount, permission: 'active' }]
    const account = contract
    const result = await api.transact({
      actions: [
        {
          account,
          authorization,
          name: 'transtake',
          data: {
            from: targetAccount,
            to: toAccount,
            quantity: '5000000.0000 BOID',
            time_limit: 1
          }
        }
      ]
    }, tapos)
    console.log(result.transaction_id)
    const bal = await boidjs.get.balance(toAccount)
    console.log('current balance:', parseFloat(bal))
    const stakes = await boidjs.get.stakes(toAccount)
    console.log('Stakes:', stakes)
  } catch (error) {
    console.log(error)
    return Promise.reject(error.toString())
  }
}

async function transfer (from,to,quantity) {
  try {
    quantity = parseFloat(quantity).toFixed(4) + " BOID"
    const contract = acct('token')
    const authorization = [{ actor: from, permission: 'active' }]
    const account = contract
    const result = await api.transact({
      actions: [
        {
          account,
          authorization,
          name: 'transfer',
          data: {
            from,
            to,
            quantity,
            memo: 'trnsfr'
          }
        }
      ]
    }, tapos)
    console.log(result.transaction_id)
    const bal = await boidjs.get.balance(from)
    console.log('From balance:', parseFloat(bal))
    const bal2 = await boidjs.get.balance(to)
    console.log('To balance:', parseFloat(bal2))
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

async function open (targetAccount) {
  try {
    const contract = acct('token')
    const authorization = [{ actor: targetAccount, permission: 'active' }]
    const account = contract
    const result = await api.transact({
      actions: [
        {
          account,
          authorization,
          name: 'open',
          data: {
            owner: targetAccount,
            symbol: '4,BOID',
            ram_payer: targetAccount
          }
        }
      ]
    }, tapos)
    console.log(result.transaction_id)
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

const methods = { claim, stake, unstake, transstake, transfer, open }

module.exports = methods

if (process.argv[2]) {
  if (Object.keys(methods).find(el => el === process.argv[2])) {
    console.log('Starting:', process.argv[2])
    methods[process.argv[2]](process.argv[3], process.argv[4], process.argv[5], process.argv[6]).catch(err => console.error(err.toString()))
      .then(() => console.log('Finished'))
  }
}
