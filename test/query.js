const env = require('../.env.js')
const accts = require('../accounts')
const acct = (name) => accts[env.network][name]
const { api, rpc, boidjs } = require('../eosjs')(env.keys[env.network])

boidjs.get.powerStats('demouser1').then(console.log)

// boidjs.get.wallet('boid').then(wallet => {
//   boidjs.get.pendingClaim(wallet)
//   .then(console.log)
// })
