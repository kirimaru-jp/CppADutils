
#define EIGEN_MATRIX_PLUGIN <eigen_plugin.h>
#define EIGEN_SPARSEMATRIXBASE_PLUGIN <eigen_sparse_plugin.h>

// #define CPPAD_USE_CPLUSPLUS_2011 1

#include <RcppEigen.h>
#include <Eigen/Eigen>
#include <Eigen/Sparse>
#include <Eigen/Cholesky>
#include <cppad/cppad.hpp>
#include <except.h>
#include <MVN_AD.h>


using Rcpp::NumericVector;
using Rcpp::NumericMatrix;
using Rcpp::as;
using Rcpp::List;
using Rcpp::Named;
using Rcpp::wrap;
using Eigen::VectorXd;
using Eigen::MatrixXd;
using Eigen::MatrixBase;
using Rcpp::Rcout;
using Eigen::Dynamic;


typedef CppAD::AD<double> AScalar;
typedef Eigen::Matrix<AScalar, Dynamic, Dynamic> MatrixXA;
typedef Eigen::Matrix<AScalar, Dynamic, 1> VectorXA;
typedef Eigen::SparseMatrix<AScalar> SparseMatrixXA;

//' @title MVN test
//' @param X_ matrix
//' @param mu_ matrix
//' @param G_ full covariance or precision matrix
//' @param isPrec covariance or precision matrix?
//' @return Numeric vector
//' @export
//[[Rcpp::export]]
NumericVector MVN_test(NumericMatrix X_, NumericMatrix mu_,
		       NumericMatrix G_, bool isPrec){
	

  size_t k = X_.rows();
  size_t N = X_.cols();
  size_t q = mu_.cols();
  //  assert(k == mu_.rows());

  MatrixXA X = MatrixXd::Map(X_.begin(), k, N).cast<AScalar>();
  MatrixXA mu = MatrixXd::Map(mu_.begin(), k, q).cast<AScalar>();
  MatrixXA G = MatrixXd::Map(G_.begin(), k, k).cast<AScalar>();

  Eigen::LLT<MatrixXA> chol_G(G);
  MatrixXA chol_GL = chol_G.matrixL();

  VectorXA out(N);

  MVN_logpdf(X, mu, chol_GL, out, isPrec);

  NumericVector res(N);
  for (size_t i=0; i<N; i++)
    res(i) = Value(out(i));
  
  return(res);
}


//[[Rcpp::export]]
NumericVector Sparse_MVN_test(NumericMatrix X_, NumericMatrix mu_,
			      Rcpp::S4 S_, bool isPrec){
  
  typedef Eigen::MappedSparseMatrix<double> MSparseXd;
  typedef Eigen::PermutationMatrix<Dynamic, Dynamic, int> Perm;
  
  size_t k = X_.rows();
  size_t N = X_.cols();
  size_t q = mu_.cols();
  //  assert(k == mu_.rows());

  MatrixXA X = MatrixXd::Map(X_.begin(), k, N).cast<AScalar>();
  MatrixXA mu = MatrixXd::Map(mu_.begin(), k, q).cast<AScalar>();

  SparseMatrixXA S = as<MSparseXd>(S_).cast<AScalar>();
  SimplicialLLT<SparseMatrixXA> ChS(S);
  Perm P = ChS.permutationP();
  SparseMatrixXA L = ChS.matrixL();

  VectorXA out(N);

  MVN_logpdf(X, mu, L, P, out, isPrec);

  NumericVector res(N);
  for (size_t i=0; i<N; i++)
    res(i) = Value(out(i));
  
  return(res);
}
  
  
/* R
X = matrix(1:4, 2)
mu = matrix((1:4)/10, 2)
G = matrix((1:4)/2, 2)
xv = (1:4)/8
CppAD_MVN_test(X, mu, G, FALSE, xv)
*/
//' @export
//[[Rcpp::export]]
List CppAD_MVN_test(NumericMatrix& X_, NumericMatrix& mu_,
		    NumericMatrix& G_, bool isPrec, NumericVector& xv_)
{ 
  int k = X_.rows();
  int N = X_.cols();
	
  
  Rcpp::NumericMatrix muR = Rcpp::as<Rcpp::NumericMatrix>(mu_);
  MatrixXd mumu = MatrixXd::Map(muR.begin(), muR.rows(), muR.cols());
  MatrixXA mu = mumu.cast<AScalar>();
 

  NumericVector xvR = as<NumericVector>(xv_);
  VectorXd xx = as<VectorXd>(xvR);
  VectorXA xv = as<VectorXd>(xvR).cast<AScalar>();
 
  
  // Standard Cholesky decomposition
  // [https://eigen.tuxfamily.org/dox/classEigen_1_1LLT.html]
  MatrixXA G = MatrixXd::Map(G_.begin(), k, k).cast<AScalar>();
  Eigen::LLT<MatrixXA> chol_G(G);
  MatrixXA chol_GL = chol_G.matrixL();
  
 
  VectorXd q(N);
  MatrixXd xz = MatrixXd::Map(xx.derived().data(),k, N); 
  // MVN_logpdf(xz, mumu, L, P, q, isPrec);

  CppAD::Independent(xv);
  MatrixXA x = MatrixXA::Map(xv.derived().data(),k,N);
  VectorXA z(N);
  VectorXA y(1);

  MVN_logpdf(x, mu, chol_GL, z, isPrec);
  y(0) = z.sum();
  
  CppAD::ADFun<double> tape;
  tape.Dependent(xv,y);
  tape.optimize();
  tape.check_for_nan(true);
 
  VectorXd f = tape.Forward(0,xx);
  VectorXd df = tape.Jacobian(xx);
  VectorXd ht = tape.Hessian(xx,0);
  MatrixXd hess = MatrixXd::Map(ht.data(),k*N,k*N);


  List res = List::create(
			  Named("f_direct")=wrap(q.sum()),
			  Named("f_AD")=wrap(f),
			  Named("df_AD")=wrap(df),
			  Named("hess_AD")=wrap(hess)
			  );
 
  return(res);
}
