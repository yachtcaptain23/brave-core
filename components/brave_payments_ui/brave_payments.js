/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const React = require('react')
const { render } = require('react-dom')
const { Provider } = require('react-redux')
const App = require('./components/app')
const { bindActionCreators } = require('redux')

window.cr.define('brave_payments', function () {
  'use strict'

  function initialize () {
    const store = require('./store')
    render(
      <Provider store={store}>
        <App />
      </Provider>,
      document.getElementById('root'))
    window.i18nTemplate.process(window.document, window.loadTimeData)
  }

  /*
  function statsUpdated (result) {
    const store = require('./store')
    const paymentsActions = require('./actions/paymentsActions')
    const actions = bindActionCreators(paymentsActions, store.dispatch.bind(store))
    actions.statsUpdated()
  }
  */

  return {
    initialize,
    // statsUpdated
  }
})

document.addEventListener('DOMContentLoaded', window.brave_payments.initialize)
