// @HEADER
// ************************************************************************
//
//               Rapid Optimization Library (ROL) Package
//                 Copyright (2014) Sandia Corporation
//
// Under terms of Contract DE-AC04-94AL85000, there is a non-exclusive
// license for use of this work by or on behalf of the U.S. Government.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact lead developers:
//              Drew Kouri   (dpkouri@sandia.gov) and
//              Denis Ridzal (dridzal@sandia.gov)
//
// ************************************************************************
// @HEADER

/*! \file  test_01.cpp
    \brief Test of StdLinearOperator, its inverse and transpose

    \f$ A=\begin{pmatrix} 4 & 1 \\ 2 & 3 \end{pmatrix},\quad 
        A^{-1}=\frac{1}{10}\begin{pmatrix} 4 & -1 \\ -2 & 3 \end{pmatrix} \f$

    1) Compute \f$b\f$ in \f$Ax = b\f$, when \f$ x=\begin{pmatrix} 1 \\ -1 \end{pmatrix}\f$
 
    2) Solve for \f$x\f$ in the above when \f$b=\begin{pmatrix} 3 \\ -1 \end{pmatrix}\f$

    3) Compute \f$c\f$ in \f$A^\top y=c\f$ when \f$y=\begin{pmatrix} -2 \\ 1 \end{pmatrix}\f$ 

    4) Solve for \f$y\f$ in the above when \f$c=\begin{pmatrix} -6 \\ 1 \end{pmatrix}\f$

    Also ensure that the interface works with both ROL::Vector and std::vector arguments
*/

#include "ROL_StdLinearOperator.hpp"
#include "Teuchos_oblackholestream.hpp"
#include "Teuchos_GlobalMPISession.hpp"

typedef double RealT;

int main(int argc, char *argv[]) {

  using Teuchos::RCP; using Teuchos::rcp;

  typedef std::vector<RealT>            vector;

  typedef ROL::StdVector<RealT>         SV;

  typedef ROL::StdLinearOperator<RealT> StdLinearOperator;


  Teuchos::GlobalMPISession mpiSession(&argc, &argv);

  // This little trick lets us print to std::cout only if a (dummy) command-line argument is provided.
  int iprint     = argc - 1;
  Teuchos::RCP<std::ostream> outStream;
  Teuchos::oblackholestream bhs; // outputs nothing
  if (iprint > 0)
    outStream = Teuchos::rcp(&std::cout, false);
  else
    outStream = Teuchos::rcp(&bhs, false);

  // Save the format state of the original std::cout.
  Teuchos::oblackholestream oldFormatState;
  oldFormatState.copyfmt(std::cout);

  int errorFlag  = 0;

  // *** Test body.

  try {

    RCP<vector> a_rcp  = rcp( new vector {4.0,2.0,1.0,3.0} );
    RCP<vector> ai_rcp = rcp( new vector {4.0/10.0, -2.0/10.0, -1.0/10.0, 3.0/10.0} );

    RCP<vector> x1_rcp  = rcp( new vector {1.0,-1.0} );
    RCP<vector> b1_rcp = rcp( new vector(2) );

    RCP<vector> x2_rcp = rcp( new vector(2) );
    RCP<vector> b2_rcp  = rcp( new vector {3.0,-1.0} );
    
    RCP<vector> y3_rcp = rcp( new vector {-2.0,1.0}  );
    RCP<vector> c3_rcp = rcp( new vector(2) );

    RCP<vector> y4_rcp = rcp( new vector(2) );
    RCP<vector> c4_rcp = rcp( new vector {-6.0,1.0} );

    StdLinearOperator A(a_rcp,false);
    StdLinearOperator At(a_rcp,true);

    SV x1(x1_rcp); SV x2(x2_rcp); SV y3(y3_rcp); SV y4(y4_rcp);
    SV b1(b1_rcp); SV b2(b2_rcp); SV c3(c3_rcp); SV c4(c4_rcp);

    RealT tol = ROL::ROL_EPSILON<RealT>();

    // Test 1
    *outStream << "\nTest 1: Matrix multiplication" << std::endl;
    A.apply(b1,x1,tol);
    *outStream << "x = [" << (*x1_rcp)[0] << "," << (*x1_rcp)[1] << "]" << std::endl;
    *outStream << "b = [" << (*b1_rcp)[0] << "," << (*b1_rcp)[1] << "]" << std::endl;
    b1.axpy(-1.0,b2);

    RealT error1 = b1.norm();
    errorFlag += error1 > tol;
    *outStream << "Error = " << error1 << std::endl;

    // Test 2    
    *outStream << "\nTest 2: Linear solve" << std::endl;
    A.applyInverse(*x2_rcp,*b2_rcp,tol);
    *outStream << "x = [" << (*x2_rcp)[0] << "," << (*x2_rcp)[1] << "]" << std::endl;
    *outStream << "b = [" << (*b2_rcp)[0] << "," << (*b2_rcp)[1] << "]" << std::endl;
    x2.axpy(-1.0,x1);

    RealT error2 = x2.norm();
    errorFlag += error2 > tol;
    *outStream << "Error = " << error2 << std::endl;

    // Test 3
    *outStream << "\nTest 3: Transposed matrix multiplication" << std::endl;
    At.apply(*c3_rcp,*y3_rcp,tol);
    *outStream << "y = [" << (*y3_rcp)[0] << "," << (*y3_rcp)[1] << "]" << std::endl;
    *outStream << "c = [" << (*c3_rcp)[0] << "," << (*c3_rcp)[1] << "]" << std::endl;
    c3.axpy(-1.0,c4);

    RealT error3 = c3.norm();
    errorFlag += error3 > tol;
    *outStream << "Error = " << error3 << std::endl;

    // Test 4
    *outStream << "\nTest 4: Linear solve with transpose" << std::endl;
    At.applyInverse(y4,c4,tol);
    *outStream << "y = [" << (*y4_rcp)[0] << "," << (*y4_rcp)[1] << "]" << std::endl;
    *outStream << "c = [" << (*c4_rcp)[0] << "," << (*c4_rcp)[1] << "]" << std::endl;
    y4.axpy(-1.0,y3);

    RealT error4 = y4.norm();
    errorFlag += error4 > tol;
    *outStream << "Error = " << error4 << std::endl;

  }
  catch (std::logic_error err) {
    *outStream << err.what() << "\n";
    errorFlag = -1000;
  }; // end try

  if (errorFlag != 0)
    std::cout << "End Result: TEST FAILED\n";
  else
    std::cout << "End Result: TEST PASSED\n";

  // reset format state of std::cout
  std::cout.copyfmt(oldFormatState);

  return 0;

}

