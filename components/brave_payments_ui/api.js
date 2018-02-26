/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { bindActionCreators } = require('redux')
const paymentsActions = require('./actions/paymentsActions')

let actions
const getActions = () => {
  if (actions) {
    return actions
  }
  const store = require('./store')
  actions = bindActionCreators(paymentsActions, store.dispatch.bind(store))
  return actions
}
