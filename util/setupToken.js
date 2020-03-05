const eosjs = require('../eosjs')
const env = require('../.env')
const accts = require('../accounts')
const acct = (name) => accts[env.network][name]
const per = require('./permissions')
const api = eosjs(env.keys[env.network]).api
const tapos = { blocksBehind: 6, expireSeconds: 10 }
const setGlobals = require('./setGlobals')
const configVars = require('./config')

async function permissions(){
  try {
    const contract = acct('token')
    const authorization = [{actor:contract,permission: 'active'}]
    const account = 'eosio'
    const result = await api.transact({
      actions: [
        { account, name: "updateauth", data:{
          account:contract, permission:"active", parent:"owner", auth:per.eosioCode(acct('token'),env.pubkeys[env.network])
        }, authorization }
      ]
    },tapos)
    console.log(result.transaction_id)
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

async function stats(){
  try {
    const contract = acct('token')
    const authorization = [{actor:contract,permission: 'active'}]
    const account = contract
    const result = await api.transact({
      actions: [
        { account,authorization,name:"create",data:{issuer:contract,maximum_supply:"25000000000.0000 BOID"}},
        { account, name: "initstats", data:{wpf_reset:false}, authorization },
      ]
    },tapos)
    console.log(result.transaction_id)
  } catch (error) {
    return Promise.reject(error.toString())
  }
}


async function config(){
  try {
    const contract = acct('token')
    const authorization = [{actor:contract,permission: 'active'}]

    const result = await api.transact({
      actions: setGlobals(contract,configVars)
      .concat([{authorization,account:contract,name:"setwpfproxy",data:{wpf_proxy:"boidworkfund"}}])      
    },tapos)
    console.log(result.transaction_id)
  } catch (error) {
    return Promise.reject(error.toString())
  }
}




const methods = {permissions,stats,config}

module.exports = methods

if (process.argv[2]) {
  if (Object.keys(methods).find(el => el === process.argv[2])) {
    console.log("Starting:",process.argv[2])
    methods[process.argv[2]]().catch(console.error)
    .then(()=>console.log('Finished'))
  }
}