/*
 *  oh_component.cpp
 *  hector
 *
 *  Created by Corinne on 11/20/2014.
 *
 */
// changed back to using only concentrations

#include <math.h>
#include "components/oh_component.hpp"
#include <boost/lexical_cast.hpp>
#include "core/core.hpp"
#include "h_util.hpp"
#include "visitors/avisitor.hpp"

namespace Hector {
  
using namespace std;

//------------------------------------------------------------------------------
/*! \brief Constructor
 */
OHComponent::OHComponent() {
    NOX_emissions.allowInterp( true ); 
    NMVOC_emissions.allowInterp( true ); 
    CO_emissions.allowInterp( true ); 
    TAU_OH.allowInterp( true );
	//TOH0.set( 0.0, U_YRS );
        
}

//------------------------------------------------------------------------------
/*! \brief Destructor
 */
OHComponent::~OHComponent() {
}

//------------------------------------------------------------------------------
// documentation is inherited
string OHComponent::getComponentName() const {
    const string name = OH_COMPONENT_NAME;
    
    return name;
}

//------------------------------------------------------------------------------
// documentation is inherited
void OHComponent::init( Core* coreptr ) {
    logger.open( getComponentName(), false, Logger::DEBUG );
    H_LOG( logger, Logger::DEBUG ) << "hello " << getComponentName() << std::endl;
    core = coreptr;

     // Inform core what data we can provide
    core->registerCapability( D_LIFETIME_OH, getComponentName() );
                    
}

//------------------------------------------------------------------------------
// documentation is inherited
unitval OHComponent::sendMessage( const std::string& message,
                                  const std::string& datum,
                                  const message_data info ) throw ( h_exception )
{
    unitval returnval;
    
    if( message==M_GETDATA ) {          //! Caller is requesting data
        return getData( datum, info.date );
        
    } else if( message==M_SETDATA ) {   //! Caller is requesting to set data
        //TODO: call setData below
        //TODO: change core so that parsing is routed through sendMessage
        //TODO: make setData private
        
    } else {                        //! We don't handle any other messages
        H_THROW( "Caller sent unknown message: "+message );
    }
    
    return returnval;
}

//------------------------------------------------------------------------------
// documentation is inherited
void OHComponent::setData( const string& varName,
                            const message_data& data ) throw ( h_exception )
{
    using namespace boost;
    try {
         if( varName == D_EMISSIONS_NOX ) {
            H_ASSERT( data.date != Core::undefinedIndex(), "date required" );
            NOX_emissions.set( data.date, unitval::parse_unitval( data.value_str, data.units_str, U_TG_N ) );
         } else if( varName == D_EMISSIONS_CO ) {
            H_ASSERT( data.date != Core::undefinedIndex(), "date required" );
            CO_emissions.set( data.date, unitval::parse_unitval( data.value_str, data.units_str, U_TG_CO ) );
         } else if( varName == D_EMISSIONS_NMVOC ) {
            H_ASSERT( data.date != Core::undefinedIndex(), "date required" );
            NMVOC_emissions.set( data.date, unitval::parse_unitval( data.value_str, data.units_str, U_TG_NMVOC ) );
        } else if( varName == D_INITIAL_LIFETIME_OH ) {
            H_ASSERT( data.date == Core::undefinedIndex(), "date not allowed" );
            TOH0 = unitval::parse_unitval( data.value_str, data.units_str, U_YRS );
         } else if(  varName == D_COEFFICENT_CH4 ) {
            H_ASSERT( data.date == Core::undefinedIndex() , "date not allowed" );
            CCH4 = lexical_cast<double>( data.value_str );
         } else if( varName == D_COEFFICENT_CO ) {
            H_ASSERT( data.date == Core::undefinedIndex(), "date not allowed" );
            CCO = lexical_cast<double>( data.value_str );
         } else if( varName == D_COEFFICENT_NMVOC ) {
            H_ASSERT( data.date == Core::undefinedIndex(), "date not allowed" );
            CNMVOC = lexical_cast<double>( data.value_str );
         } else if( varName == D_COEFFICENT_NOX ) {
            H_ASSERT( data.date == Core::undefinedIndex(), "date not allowed" );
            CNOX = lexical_cast<double>( data.value_str );
         }	else {
            H_THROW( "Unknown variable name while parsing " + getComponentName() + ": "
                    + varName );
        }
    } catch( h_exception& parseException ) {
        H_RETHROW( parseException, "Could not parse var: "+varName );
    }
}

//------------------------------------------------------------------------------
// documentation is inherited
void OHComponent::prepareToRun() throw ( h_exception ) {
    
    H_LOG( logger, Logger::DEBUG ) << "prepareToRun " << std::endl;
	oldDate = core->getStartDate();
    //get intial CH4 concentration
     M0 = core->sendMessage( M_GETDATA, D_PREINDUSTRIAL_CH4 );
    TAU_OH.set( oldDate, TOH0 );
    
 }

//------------------------------------------------------------------------------
// documentation is inherited
void OHComponent::run( const double runToDate ) throw ( h_exception ) {
	H_ASSERT( !core->inSpinup() && runToDate-oldDate == 1, "timestep must equal 1" );

       // modified from Tanaka et al 2007 and Wigley et al 2002.
    unitval current_nox = NOX_emissions.get( runToDate ); 
    unitval current_co = CO_emissions.get( runToDate ); 
    unitval current_nmvoc = NMVOC_emissions.get( runToDate ); 
    
    //get this from CH4 component, this is last year's value
   const double previous_ch4 = core->sendMessage( M_GETDATA, D_ATMOSPHERIC_CH4, oldDate ).value( U_PPBV_CH4 );
       
   double toh = 0.0;
   if ( previous_ch4 != M0 ) // if we are not at the first time
   {
   const double a =  CCH4 * ( ( -1.0 * log( previous_ch4 ) ) + log( M0.value(U_PPBV_CH4) ) );
   const double b = CNOX * ( ( -1.0 * current_nox ) + NOX_emissions.get( NOX_emissions.firstdate() ).value( U_TG_N ) );
   const double c = CCO * ( ( -1.0 * + current_co ) + CO_emissions.get( CO_emissions.firstdate() ).value( U_TG_CO ) );
   const double d = CNMVOC * ( (-1.0 * + current_nmvoc ) + NMVOC_emissions.get( NMVOC_emissions.firstdate() ).value( U_TG_NMVOC ) );
    toh = a + b + c + d;
    H_LOG( logger, Logger::DEBUG ) << "Year " << runToDate << " toh = " << toh << std::endl;
   }
    
   TAU_OH.set( runToDate,  TOH0 * exp( toh ) );

    oldDate = runToDate;       
    H_LOG( logger, Logger::DEBUG ) << "Year " << runToDate << " OH lifetime = " << TAU_OH.get( runToDate ) << std::endl;
}

//------------------------------------------------------------------------------
// documentation is inherited
unitval OHComponent::getData( const std::string& varName,
                              const double date ) throw ( h_exception ) {
    
    unitval returnval;
    
    if( varName == D_LIFETIME_OH ) {
        H_ASSERT( date != Core::undefinedIndex(), "Date required for OH lifetime" ); 
        returnval = TAU_OH.get( date );
      } else {
        H_THROW( "Caller is requesting unknown variable: " + varName );
    }
    
    return returnval;
}

//------------------------------------------------------------------------------
// documentation is inherited
void OHComponent::shutDown() {
	H_LOG( logger, Logger::DEBUG ) << "goodbye " << getComponentName() << std::endl;
    logger.close();
}

//------------------------------------------------------------------------------
// documentation is inherited
void OHComponent::accept( AVisitor* visitor ) {
    visitor->visit( this );
}

}