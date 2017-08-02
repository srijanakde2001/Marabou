/*********************                                                        */
/*! \file ReluConstraint.cpp
 ** \verbatim
 ** Top contributors (to current version):
 **   Guy Katz
 ** This file is part of the Marabou project.
 ** Copyright (c) 2016-2017 by the authors listed in the file AUTHORS
 ** in the top-level source directory) and their institutional affiliations.
 ** All rights reserved. See the file COPYING in the top-level source
 ** directory for licensing information.\endverbatim
 **/

#include "Debug.h"
#include "FloatUtils.h"
#include "FreshVariables.h"
#include "ITableau.h"
#include "PiecewiseLinearCaseSplit.h"
#include "ReluConstraint.h"
#include "ReluplexError.h"

ReluConstraint::ReluConstraint( unsigned b, unsigned f )
    : _b( b )
    , _f( f )
{
    // split 0 = active phase, split 1 = inactive phase

    // Auxiliary variable bound, needed for either phase
    unsigned auxVariable = FreshVariables::getNextVariable();
    Tightening auxUpperBound( auxVariable, 0.0, Tightening::UB );
    Tightening auxLowerBound( auxVariable, 0.0, Tightening::LB );

    // Active phase: b >= 0, b - f = 0
    PiecewiseLinearCaseSplit activePhase;
    Tightening activeBound( _b, 0.0, Tightening::LB );
    activePhase.storeBoundTightening( activeBound );
    Equation activeEquation;
    activeEquation.addAddend( 1, _b );
    activeEquation.addAddend( -1, _f );
    activeEquation.addAddend( 1, auxVariable );
    activeEquation.markAuxiliaryVariable( auxVariable );
    activeEquation.setScalar( 0 );
    activePhase.addEquation( activeEquation );
    activePhase.storeBoundTightening( auxUpperBound );
    activePhase.storeBoundTightening( auxLowerBound );
    _splits.append( activePhase );

    // Inactive phase: b <= 0, f = 0
    PiecewiseLinearCaseSplit inactivePhase;
    Tightening inactiveBound( _b, 0.0, Tightening::UB );
    inactivePhase.storeBoundTightening( inactiveBound );
    Equation inactiveEquation;
    inactiveEquation.addAddend( 1, _f );
    inactiveEquation.addAddend( 1, auxVariable );
    inactiveEquation.markAuxiliaryVariable( auxVariable );
    inactiveEquation.setScalar( 0 );
    inactivePhase.addEquation( inactiveEquation );
    inactivePhase.storeBoundTightening( auxUpperBound );
    inactivePhase.storeBoundTightening( auxLowerBound );
    _splits.append( inactivePhase );

    // Initially, we could be in either phase
    _validSplits.append( _splits[0] );
    _validSplits.append( _splits[1] );
}

void ReluConstraint::registerAsWatcher( ITableau *tableau )
{
    _tableau = tableau;
    tableau->registerToWatchVariable( this, _b );
    tableau->registerToWatchVariable( this, _f );
}

void ReluConstraint::unregisterAsWatcher( ITableau *tableau )
{
    ASSERT( _tableau == tableau );
    tableau->unregisterToWatchVariable( this, _b );
    tableau->unregisterToWatchVariable( this, _f );
    _tableau = NULL;
}

void ReluConstraint::notifyVariableValue( unsigned variable, double value )
{
    _assignment[variable] = value;
}

void ReluConstraint::notifyLowerBound( unsigned variable, double bound )
{
    _lowerBounds[variable] = bound;
    if ( (variable == _b || variable == _f) && FloatUtils::isPositive( bound ) )
    {
        // stuck in active phase
        _validSplits.clear();
        _validSplits.append( _splits[0] );
        ASSERT( _tableau );
        _tableau->applySplit( _splits[0] );
    }
}

void ReluConstraint::notifyUpperBound( unsigned variable, double bound )
{
    _upperBounds[variable] = bound;
    if ( (variable == _f) && FloatUtils::isNegative( bound ) )
    {
        // stuck in inactive phase
        _validSplits.clear();
        _validSplits.append( _splits[1] );
        ASSERT( _tableau );
        _tableau->applySplit( _splits[1] );
    }
}

bool ReluConstraint::participatingVariable( unsigned variable ) const
{
    return ( variable == _b ) || ( variable == _f );
}

List<unsigned> ReluConstraint::getParticiatingVariables() const
{
    return List<unsigned>( { _b, _f } );
}

bool ReluConstraint::satisfied() const
{
    if ( !( _assignment.exists( _b ) && _assignment.exists( _f ) ) )
        throw ReluplexError( ReluplexError::PARTICIPATING_VARIABLES_ABSENT );

    double bValue = _assignment.get( _b );
    double fValue = _assignment.get( _f );

    ASSERT( !FloatUtils::isNegative( fValue ) );

    if ( FloatUtils::isPositive( fValue ) )
        return FloatUtils::areEqual( bValue, fValue );
    else
        return !FloatUtils::isPositive( bValue );
}

List<PiecewiseLinearConstraint::Fix> ReluConstraint::getPossibleFixes() const
{
    ASSERT( !satisfied() );
    ASSERT( _assignment.exists( _b ) );
    ASSERT( _assignment.exists( _f ) );

    double bValue = _assignment.get( _b );
    double fValue = _assignment.get( _f );

    ASSERT( !FloatUtils::isNegative( fValue ) );

    List<PiecewiseLinearConstraint::Fix> fixes;

    // Possible violations:
    //   1. f is positive, b is positive, b and f are disequal
    //   2. f is positive, b is non-positive
    //   3. f is zero, b is positive
    if ( FloatUtils::isPositive( fValue ) )
    {
        if ( FloatUtils::isPositive( bValue ) )
        {
            fixes.append( PiecewiseLinearConstraint::Fix( _b, fValue ) );
            fixes.append( PiecewiseLinearConstraint::Fix( _f, bValue ) );
        }
        else
        {
            fixes.append( PiecewiseLinearConstraint::Fix( _b, fValue ) );
            fixes.append( PiecewiseLinearConstraint::Fix( _f, 0 ) );
        }
    }
    else
    {
        fixes.append( PiecewiseLinearConstraint::Fix( _b, 0 ) );
        fixes.append( PiecewiseLinearConstraint::Fix( _f, bValue ) );
    }

    return fixes;
}

List<PiecewiseLinearCaseSplit> ReluConstraint::getCaseSplits() const
{
    return _validSplits;
}

//
// Local Variables:
// compile-command: "make -C .. "
// tags-file-name: "../TAGS"
// c-basic-offset: 4
// End:
//
