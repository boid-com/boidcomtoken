# boidcomtoken
Deployed on Kylin: token.boid

Deployed on EOS Mainnet: boidcomtoken

Documentation available at https://docs.boid.com/docs/stake
## build Instructions

eosio-cpp boidtoken.cpp -o ../boidtoken.wasm -abigen -I ../include -I ./ -O 2 -lto-opt O2