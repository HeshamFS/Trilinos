// @HEADER
// ***********************************************************************
// 
//                    Teuchos: Common Tools Package
//                 Copyright (2004) Sandia Corporation
// 
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
// 
// This library is free software; you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as
// published by the Free Software Foundation; either version 2.1 of the
// License, or (at your option) any later version.
//  
// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//  
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA
// Questions? Contact Michael A. Heroux (maherou@sandia.gov) 
// 
// ***********************************************************************
// @HEADER

#ifndef TEUCHOS_ASSERT_HPP
#define TEUCHOS_ASSERT_HPP


#include "Teuchos_TestForException.hpp"


/** \brief This macro is designed to be a short version of
 * <tt>TEST_FOR_EXCEPTION()</tt> that is easier to call.
 *
 * @param  throw_exception_test
 *               [in] Test for when to throw the std::exception.  This can and
 *               should be an expression that may mean something to the user.
 *               The text verbatim of this expression is included in the
 *               formed error std::string.
 *
 * \note The std::exception thrown is <tt>std::logic_error</tt>.
 *
 * \ingroup TestForException_grp
 */
#define TEUCHOS_ASSERT(assertion_test) TEST_FOR_EXCEPT(!(assertion_test))


/** \brief This macro is designed to be a short version of
 * <tt>TEST_FOR_EXCEPTION()</tt> that is easier to call.
 *
 * @param  throw_exception_test
 *               [in] Test for when to throw the std::exception.  This can and
 *               should be an expression that may mean something to the user.
 *               The text verbatim of this expression is included in the
 *               formed error std::string.
 *
 * \note The std::exception thrown is <tt>std::logic_error</tt>.
 *
 * \ingroup TestForException_grp
 */
#define TEUCHOS_ASSERT_IN_RANGE_UPPER_EXCLUSIVE( index, lower_inclusive, upper_exclusive ) \
  { \
    TEST_FOR_EXCEPTION( \
      !( (lower_inclusive) <= (index) && (index) < (upper_exclusive) ), \
      std::out_of_range, \
      "Error, the index " #index " = " << (index) << " does not fall in the range" \
      "["<<(lower_inclusive)<<","<<(upper_exclusive)<<")!" ); \
  }

#endif // TEUCHOS_ASSERT_HPP
