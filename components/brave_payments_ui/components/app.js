/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const React = require('react')
const { DragDropContext } = require('react-dnd')
const { bindActionCreators } = require('redux')
const { connect } = require('react-redux')
const newTabActions = require('../actions/paymentsActions')

class PaymentsPage extends React.Component {
  constructor (props) {
    super(props)
    this.onBackgroundImageLoadFailed = this.onBackgroundImageLoadFailed.bind(this)
    this.onUndoAllSiteIgnored = this.onUndoAllSiteIgnored.bind(this)
    this.onUndoIgnoredTopSite = this.onUndoIgnoredTopSite.bind(this)
    this.onHideSiteRemovalNotification = this.onHideSiteRemovalNotification.bind(this)
    this.onDraggedSite = this.onDraggedSite.bind(this)
    this.onDragEnd = this.onDragEnd.bind(this)
  }

  render () {
    // const { paymentsData } = this.props
    return <div className='empty' />
  }
}

const mapStateToProps = (state) => ({
  paymentsData: state.paymentsData
})

const mapDispatchToProps = (dispatch) => ({
  actions: bindActionCreators(paymentsActions, dispatch)
})

export default connect(
  mapStateToProps,
  mapDispatchToProps
)(DragDropContext(HTML5Backend)(NewTabPage))
