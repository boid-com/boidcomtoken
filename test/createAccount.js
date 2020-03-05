const eosjs = require('../eosjs')
const env = require('../.env')
const pubkey = 'EOS7PE114KQs5MamHcH4LZTZN57NLwFekAQs89zzfMVkLfBtrxoZA'
const randomstring = require('randomstring')

function randomName () {
  const rand = randomstring.generate({ length: 20 })
    .toLowerCase().split('')
    .filter(el => {
      const result = el.search('(^[a-z1-5.]{0,11}[a-z1-5]$)|(^[a-z1-5.]{12}[a-j1-5]$)')
      if (result != 0) return false
      else return true
    }).join('')
  return rand.substring(0, 12)
}

async function createAccount (accountName) {
  try {
    if (!accountName) accountName = randomName()
    const creator = 'eosio'
    var createData = { creator, name: accountName }
    createData.owner = { threshold: 1, keys: [{ key: pubkey, weight: 1 }], accounts: [], waits: [] }
    createData.active = createData.owner
    const tapos = { blocksBehind: 12, expireSeconds: 30 }
    const authorization = [{ actor: creator, permission: 'active' }]
    const account = 'eosio'
    const api = eosjs(env.keys[env.network]).api
    const accountCreated = await api.transact({ actions: [{ account, name: 'newaccount', data: createData, authorization }] }, tapos)
    delete accountCreated.processed.action_traces
    console.log('Account Created:', accountName)
    return accountName
  } catch (error) {
    return Promise.reject(error.toString())
  }
}

module.exports = createAccount

if (require.main === module && process.argv[2]) createAccount(process.argv[2]).catch(console.log)
