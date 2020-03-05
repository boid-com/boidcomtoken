const env = require('../.env')
const { api, rpc, boidjs } = require('../eosjs')(env.keys[env.network])
const config = require('../util/config')

async function init (lastClaimTime, totalPower, allStaked) {
  const result = boidjs.get.pendingClaim({ lastClaimTime, totalPower, allStaked }, config)
  console.log(result)
  return result
}

if (require.main === module && process.argv[2]) init(parseFloat(process.argv[2]), parseFloat(process.argv[3]), parseFloat(process.argv[4])).catch(console.log)
