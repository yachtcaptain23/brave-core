import { combineReducers } from 'redux'
import paymentsReducer from './paymentsReducer'

const combinedReducer = combineReducers({
  paymentsData: paymentsReducer
})

export default combinedReducer
