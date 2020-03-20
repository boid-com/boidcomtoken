const env = require('../.env.js')
const accts = require('../accounts')
const acct = (name) => accts[env.network][name]
const { api, rpc, boidjs } = require('../eosjs')(env.keys[env.network])
const tapos = { blocksBehind: 6, expireSeconds: 60 }

const targetAccount = 'boid'

async function issue (targetAccount, amount) {
  try {
    const quantity = parseFloat(amount).toFixed(4) + ' BOID'
    console.log(quantity)
    const contract = acct('token')
    const authorization = [{ actor: contract, permission: 'active' }]
    const account = contract
    const actions = [{ account, authorization, name: 'issue', data: { to: targetAccount, quantity, memo: 'do it' } }]
    const result = await api.transact({ actions }, tapos)
    console.log(result.transaction_id)
    const balance = await boidjs.get.balance(targetAccount)
    console.log('Current Balance:', balance)
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

async function setPower (targetAccount, boidpower) {
  try {
    const contract = acct('token')
    const authorization = [{ actor: contract, permission: 'active' }]
    const account = contract
    const result = await api.transact({
      actions: [
        { account, authorization, name: 'setpower', data: { acct: targetAccount, boidpower, reset_claim_time: true } }
      ]
    }, tapos)
    console.log('Power Stats:', result.transaction_id)
    const power = await boidjs.get.powerStats(targetAccount)
    console.log('current power:', parseFloat(power.quantity))
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

async function updatePower (targetAccount, boidpower) {
  try {
    const contract = acct('token')
    const authorization = [{ actor: contract, permission: 'active' }]
    const account = contract
    const result = await api.transact({
      actions: [
        { account, authorization, name: 'updatepower', data: { acct: targetAccount, boidpower } }
      ]
    }, tapos)
    console.log('Power Stats:', result.transaction_id)
    const power = await boidjs.get.powerStats(targetAccount)
    console.log('current power:', parseFloat(power.quantity))
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

async function claimForUser () {
  try {
    const contract = acct('token')
    const authorization = [{ actor: contract, permission: 'active' }]
    const account = contract
    const result = await api.transact({
      actions: [
        { account, authorization, name: 'claim', data: { stake_account: targetAccount, issuer_claim: true } }
      ]
    }, tapos)
    console.log(result.transaction_id)
    const power = await boidjs.get.powerStats(targetAccount)
    console.log('current power:', parseFloat(power.quantity))
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

async function unstake () {
  try {
    const targetAccount = 'boid'
    const toAccount = 'boid'
    const contract = acct('token')
    const authorization = [{ actor: contract, permission: 'active' }]
    const account = contract
    const result = await api.transact({
      actions: [
        {
          account,
          authorization,
          name: 'unstake',
          data:
          {
            from: 'john',
            to: toAccount,
            quantity: '200000.0000 BOID',
            time_limit: 0,
            issuer_unstake: true,
            transfer: true
          }
        }
      ]
    }, tapos)
    console.log(result.transaction_id)
    const bal = await boidjs.get.balance(toAccount)
    console.log('current balance:', parseFloat(bal))
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

const methods = { issue, setPower, claimForUser, unstake, updatePower }

module.exports = methods

if (process.argv[2]) {
  if (Object.keys(methods).find(el => el === process.argv[2])) {
    console.log('Starting:', process.argv[2])
    methods[process.argv[2]](process.argv[3], process.argv[4], process.argv[5], process.argv[6], process.argv[7]).catch(err => console.error(err.toString()))
      .then(() => console.log('Finished'))
  }
}
