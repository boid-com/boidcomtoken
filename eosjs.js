const { JsonRpc,Api } = require('eosjs')
const { JsSignatureProvider } = require('eosjs/dist/eosjs-jssig')
const { TextDecoder, TextEncoder } = require('util')
const env = require('./.env')
function init(keys) {
  if (!keys) keys = []
  const signatureProvider = new JsSignatureProvider(keys)
  const fetch = require('node-fetch')
  var rpc = new JsonRpc(env.endpoints[env.network].rpc, { fetch })
  const api = new Api({ rpc, signatureProvider, textDecoder: new TextDecoder(), textEncoder: new TextEncoder() })
  const boidjs = require('boidjs')({rpc,api})

  return {api,rpc,boidjs}
}



module.exports = init