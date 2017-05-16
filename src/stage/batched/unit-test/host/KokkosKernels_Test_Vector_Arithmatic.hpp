/// \author Kyungjoo Kim (kyukim@sandia.gov)


#include "Kokkos_Core.hpp"
#include "impl/Kokkos_Timer.hpp"

#include "KokkosKernels_Vector.hpp"

namespace KokkosKernels {
  namespace Batched {
    namespace Experimental {
      namespace ArithmaticTest {
    
        enum : int  { TEST_ADD = 0,
                      TEST_SUBTRACT = 1,
                      TEST_MULT = 2,
                      TEST_DIV = 3,
                      TEST_UNARY_MINUS = 4,
                      TEST_MAX = 5 };
    
        template<typename ViewType, int TestID>
        struct Functor {
          ViewType _a, _b, _c;

          KOKKOS_INLINE_FUNCTION
          Functor(const ViewType &a, 
                  const ViewType &b, 
                  const ViewType &c) 
            : _a(a), _b(b), _c(c) {}
      
          inline
          const char* label() const {
            switch (TestID) {
            case 0: return "add"; break;
            case 1: return "subtract";  break;
            case 2: return "multiply";  break;
            case 3: return "divide";  break;
            case 4: return "unary minus"; break;
            }
            return "nothing";
          }

          KOKKOS_INLINE_FUNCTION
          void operator()(const int i) const {
            switch (TestID) {
            case 0: _c(i) = _a(i) + _b(i); break;
            case 1: _c(i) = _a(i) - _b(i); break;
            case 2: _c(i) = _a(i) * _b(i); break;
            case 3: _c(i) = _a(i) / _b(i); break;
            case 4: _c(i) = -_c(i); break;
            }
          }

          inline
          int run() {
            Kokkos::RangePolicy<DeviceSpaceType> policy(0, _c.dimension_0());
            Kokkos::parallel_for(policy, *this);
          }
      
        };

        template<typename DeviceSpaceType, typename VectorTagType, int TestID>
        void VectorArithmatic() {
          constexpr int N = 32768;
            
          const int iter_begin = -10, iter_end = 100;
          Kokkos::Impl::Timer timer;
          double t = 0;

          ///
          /// random data initialization
          ///
          typedef Vector<VectorTagType> vector_type;
          constexpr int vector_length = vector_type::vector_length;
          typedef typename vector_type::value_type scalar_type;

          Kokkos::View<scalar_type*,HostSpaceType> 
            a_host("a_host", N), 
            b_host("b_host", N), 
            c_host("c_host", N);


          Kokkos::View<vector_type*,HostSpaceType> 
            avec_host("avec_host", N/vector_length), 
            bvec_host("bvec_host", N/vector_length), 
            cvec_host("cvec_host", N/vector_length);
      
          Random random;
          for (int k=0;k<N;++k) {
            a_host(k) = random.value();
            b_host(k) = random.value();
            c_host(k) = random.value();

            const int i = k/vector_length, j = k%vector_length;
            avec_host(i)[j] = a_host(k);
            bvec_host(i)[j] = b_host(k);
            cvec_host(i)[j] = c_host(k);
          }

          ///
          /// test for reference
          ///

          {
            auto aref = Kokkos::create_mirror_view(typename DeviceSpaceType::memory_space(), a_host);
            auto bref = Kokkos::create_mirror_view(typename DeviceSpaceType::memory_space(), b_host);
            auto cref = Kokkos::create_mirror_view(typename DeviceSpaceType::memory_space(), c_host);
        
            Kokkos::deep_copy(aref, a_host);
            Kokkos::deep_copy(bref, b_host);
            Kokkos::deep_copy(cref, c_host);
        
            {
              t = 0;
              Functor<decltype(cref),TestID> test(aref,bref,cref);
              for (int iter=iter_begin;iter<iter_end;++iter) {
                DeviceSpaceType::fence();
                timer.reset();
                test.run();
                DeviceSpaceType::fence();
                t += (iter >= 0)*timer.seconds();
              }
              printf("Reference,     Test %12s, Time %e\n", test.label(), (t/iter_end));
            }
            Kokkos::deep_copy(c_host, cref);

            auto avec = Kokkos::create_mirror_view(typename DeviceSpaceType::memory_space(), avec_host);
            auto bvec = Kokkos::create_mirror_view(typename DeviceSpaceType::memory_space(), bvec_host);
            auto cvec = Kokkos::create_mirror_view(typename DeviceSpaceType::memory_space(), cvec_host);
        
            Kokkos::deep_copy(avec, avec_host);
            Kokkos::deep_copy(bvec, bvec_host);
            Kokkos::deep_copy(cvec, cvec_host);
        
            {
              t = 0;
              Functor<decltype(cvec),TestID> test(avec,bvec,cvec);
              for (int iter=iter_begin;iter<iter_end;++iter) {
                DeviceSpaceType::fence();
                timer.reset();
                test.run();
                DeviceSpaceType::fence();
                t += (iter >= 0)*timer.seconds();
              }
              printf("%9s,%3d, Test %12s, Time %e\n", vector_type::label(), vector_length, test.label(), (t/iter_end));
            }
        
            Kokkos::deep_copy(cvec_host, cvec);
          }
      
          ///
          /// check cref = cvec
          ///

          double sum = 0;
          for (int k=0;k<N;++k) {
            const int i = k/vector_length, j = k%vector_length;
            const auto diff = abs(c_host(k) - cvec_host(i)[j]);
            sum += diff;
          }
          EXPECT_TRUE(sum < std::numeric_limits<double>::epsilon());
      
        }
      }
    }
  }
}

using namespace KokkosKernels::Batched::Experimental;

///
/// double vector length 4
///

TEST( VectorArithmatic, double_SIMD4 ) {
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 4>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 4>,ArithmaticTest::TEST_SUBTRACT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 4>,ArithmaticTest::TEST_MULT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 4>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 4>,ArithmaticTest::TEST_UNARY_MINUS>();
}

TEST( VectorArithmatic, dcomplex_SIMD2 ) {
  typedef std::complex<double> dcomplex;

  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 2>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 2>,ArithmaticTest::TEST_SUBTRACT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 2>,ArithmaticTest::TEST_MULT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 2>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 2>,ArithmaticTest::TEST_UNARY_MINUS>();
}

#if defined(__AVX__) || defined(__AVX2__)
TEST( VectorArithmatic, double_AVX256 ) {
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 4>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 4>,ArithmaticTest::TEST_SUBTRACT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 4>,ArithmaticTest::TEST_MULT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 4>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 4>,ArithmaticTest::TEST_UNARY_MINUS>();
}

TEST( VectorArithmatic, dcomplex_AVX256 ) {
  typedef Kokkos::complex<double> dcomplex;

  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<dcomplex>, 2>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<dcomplex>, 2>,ArithmaticTest::TEST_SUBTRACT>();
  
  // mult uses fmaddsub and I am not sure how I can detect this instruction
  //ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<dcomplex>, 2>,ArithmaticTest::TEST_MULT>();
  //ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<dcomplex>, 2>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<dcomplex>, 2>,ArithmaticTest::TEST_UNARY_MINUS>();
}
#endif

///
/// double vector length 8
///

TEST( VectorArithmatic, double_SIMD8 ) {
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 8>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 8>,ArithmaticTest::TEST_SUBTRACT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 8>,ArithmaticTest::TEST_MULT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 8>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>, 8>,ArithmaticTest::TEST_UNARY_MINUS>();
}

TEST( VectorArithmatic, dcomplex_SIMD4 ) {
  typedef std::complex<double> dcomplex;

  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 4>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 4>,ArithmaticTest::TEST_SUBTRACT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 4>,ArithmaticTest::TEST_MULT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 4>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 4>,ArithmaticTest::TEST_UNARY_MINUS>();
}

#if defined(__AVX512F__)
TEST( VectorArithmatic, AVX8 ) {
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 8>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 8>,ArithmaticTest::TEST_SUBTRACT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 8>,ArithmaticTest::TEST_MULT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 8>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<AVX<double>, 8>,ArithmaticTest::TEST_UNARY_MINUS>();
}
#endif

///
/// double vector length 64
///

TEST( VectorArithmatic, double_SIMD64 ) {
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>,64>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>,64>,ArithmaticTest::TEST_SUBTRACT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>,64>,ArithmaticTest::TEST_MULT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>,64>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<double>,64>,ArithmaticTest::TEST_UNARY_MINUS>();
}

TEST( VectorArithmatic, dcomplex_SIMD32 ) {
  typedef std::complex<double> dcomplex;

  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 32>,ArithmaticTest::TEST_ADD>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 32>,ArithmaticTest::TEST_SUBTRACT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 32>,ArithmaticTest::TEST_MULT>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 32>,ArithmaticTest::TEST_DIV>();
  ArithmaticTest::VectorArithmatic<DeviceSpaceType,VectorTag<SIMD<dcomplex>, 32>,ArithmaticTest::TEST_UNARY_MINUS>();
}