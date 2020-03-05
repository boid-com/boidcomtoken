const user = require('../user')
const contract = require('../contract')
const env = require('../../.env.js')
const accts = require('../../accounts')
const acct = (name) => accts[env.network][name]
const { api, rpc, boidjs } = require('../../eosjs')(env.keys[env.network])
const createAccount = require('../createAccount')
const sleep = ms => new Promise(res => setTimeout(res, ms))
const pendingClaim = require('../calculatePendingClaim')
const TIME_MULT = 1

async function init (targetUser) {
  try {
    let balance
    let powers
    let stake
    let pendingClaim
    let config
    let wallet
    let delegations

    boidjs.get.stakeConfig().then(configResult => config = configResult)
    if (!targetUser) targetUser = await createAccount()
    console.log('Open')
    await user.open(targetUser)
    console.log('Set Power')
    await contract.setPower(targetUser, 1)
    await contract.updatePower(targetUser, 20000)
    console.log('Sleep')
    await sleep(5000)
    wallet = await boidjs.get.wallet(targetUser)
    pendingClaim = boidjs.get.pendingClaim(wallet, config)
    // console.log('Pending Claim Power:',pendingClaim.power * TIME_MULT)
    console.log('Claim')
    await user.claim(targetUser)
    balance = await boidjs.get.balance(targetUser)
    console.log('Balance after claim:', balance)
    // return
    await contract.issueTokens(targetUser, 10000000)
    await user.stake(targetUser, targetUser, 6000000, 0)
    await user.stake(targetUser, 'boid', 1000000, 0)
    // return
    await user.stake(targetUser, targetUser, 403330, 9.000385e+10)
    await user.stake(targetUser, 'boid', 3433344, 9.000385e+10)
    stake = await boidjs.get.stakes(targetUser)
    console.log('Stakes:', stake)
    delegations = await boidjs.get.delegations(targetUser)
    console.log('delegations:', delegations)
    powers = await boidjs.get.powerStats(targetUser)
    console.log('Powers:', powers)
    await sleep(5000)
    wallet = await boidjs.get.wallet(targetUser)
    pendingClaim = boidjs.get.pendingClaim(wallet, config)
    console.log('Pending Claim Power:', pendingClaim.power * TIME_MULT)
    console.log('Claim')
    await user.claim(targetUser)
    balance = await boidjs.get.balance(targetUser)
    console.log('Balance after claim:', balance)
    powers = await boidjs.get.powerStats(targetUser)
    console.log('Powers:', powers)
  } catch (error) {
    console.log(error.toString())
  }
}

module.exports = init

if (require.main === module) init(process.argv[2]).catch(console.log)
