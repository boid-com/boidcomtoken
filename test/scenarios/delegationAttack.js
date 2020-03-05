const user = require('../user')
const contract = require('../contract')
const env = require('../../.env.js')
const accts = require('../../accounts')
const acct = (name) => accts[env.network][name]
const { api, rpc, boidjs } = require('../../eosjs')(env.keys[env.network])
const createAccount = require('../createAccount')
const sleep = ms => new Promise(res => setTimeout(res, ms))
const pendingClaim = require('../calculatePendingClaim')
const TIME_MULT = 10000

async function init (targetUser, itterations) {
  try {
    let finished = 0
    const jobs = []
    while (finished < itterations) {
      jobs.push(new Promise(async res => {
        var newAccount = await createAccount()
        await contract.issueTokens(newAccount, 10000000)
        await user.stake(newAccount, targetUser, 10000000, 0)
        return true
      }))
      finished++
    }
    await Promise.all(jobs)
    return
    if (!targetUser) targetUser = await createAccount()
    console.log('Open')
    await user.open(targetUser)
    console.log('Set Power')
    // await contract.setPower(targetUser,20000)
    await contract.updatePower(targetUser, 20000)

    console.log('Sleep')
    await sleep(5000)
    await contract.updatePower(targetUser, 20000)

    // await contract.updatePower(targetUser,10000)
    wallet = await boidjs.get.wallet(targetUser)
    pendingClaim = boidjs.get.pendingClaim(wallet, config)
    console.log('Pending Claim Power:', pendingClaim.power * TIME_MULT)
    console.log('Claim')
    await user.claim(targetUser)
    balance = await boidjs.get.balance(targetUser)
    console.log('Balance after claim:', balance)
    // return
    await contract.issueTokens(targetUser, 10000000)
    await user.stake(targetUser, targetUser, 6000000, 0)
    await user.stake(targetUser, 'boid', 1000000, 0)
    stake = await boidjs.get.stakes(targetUser)
    console.log('Stakes:', stake)
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

if (require.main === module) init(process.argv[2], process.argv[3]).catch(console.log)
