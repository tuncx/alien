#include "hypre_matrix.h"
#include "hypre_vector.h"

#include <cmath>

// FIXME: use public API for Hypre !
#include <_hypre_parcsr_mv.h>

#include <arccore/message_passing_mpi/MpiMessagePassingMng.h>
#include <arccore/base/NotImplementedException.h>
#include <arccore/base/TraceInfo.h>

#include <alien/core/backend/LinearAlgebraT.h>

#include <alien/hypre/backend.h>
#include <alien/hypre/export.h>


namespace Alien {

    namespace {
        HYPRE_ParVector hypre_implem(const Hypre::Vector &v) {
            HYPRE_ParVector res;
            HYPRE_IJVectorGetObject(v.internal(), reinterpret_cast<void **>(&res));
            return res;
        }

        HYPRE_ParCSRMatrix hypre_implem(const Hypre::Matrix &m) {
            HYPRE_ParCSRMatrix res;
            HYPRE_IJMatrixGetObject(m.internal(), reinterpret_cast<void **>(&res));
            return res;
        }
    }

    template
    class ALIEN_HYPRE_EXPORT LinearAlgebra<BackEnd::tag::hypre>;
}

namespace Alien::Hypre {

    class ALIEN_HYPRE_EXPORT InternalLinearAlgebra
            : public IInternalLinearAlgebra<Matrix, Vector> {
    public:

        InternalLinearAlgebra() {}

        virtual ~InternalLinearAlgebra() {}

    public:

        // IInternalLinearAlgebra interface.
        Arccore::Real norm0(const Vector &x) const;

        Arccore::Real norm1(const Vector &x) const;

        Arccore::Real norm2(const Vector &x) const;

        void mult(const Matrix &a, const Vector &x, Vector &r) const;

        void axpy(const Arccore::Real &alpha, const Vector &x, Vector &r) const;

        void aypx(const Arccore::Real &alpha, Vector &y, const Vector &x) const;

        void copy(const Vector &x, Vector &r) const;

        Arccore::Real dot(const Vector &x, const Vector &y) const;

        void scal(const Arccore::Real &alpha, Vector &x) const;

        void diagonal(const Matrix &a, Vector &x) const;

        void reciprocal(Vector &x) const;

        void pointwiseMult(const Vector &x, const Vector &y, Vector &w) const;
    };

    Arccore::Real
    InternalLinearAlgebra::norm0(const Vector &vx ALIEN_UNUSED_PARAM) const {
        throw Arccore::NotImplementedException(A_FUNCINFO, "HypreLinearAlgebra::norm0 not implemented");
    }

    Arccore::Real
    InternalLinearAlgebra::norm1(const Vector &vx ALIEN_UNUSED_PARAM) const {
        throw Arccore::NotImplementedException(A_FUNCINFO, "HypreLinearAlgebra::norm1 not implemented");
    }

    Arccore::Real
    InternalLinearAlgebra::norm2(const Vector &vx) const {
        return std::sqrt(dot(vx, vx));
    }

    void
    InternalLinearAlgebra::mult(const Matrix &ma, const Vector &vx, Vector &vr) const {
        HYPRE_ParCSRMatrixMatvec(1.0, hypre_implem(ma), hypre_implem(vx), 0.0, hypre_implem(vr));
    }

    void
    InternalLinearAlgebra::axpy(
            const Arccore::Real &alpha ALIEN_UNUSED_PARAM, const Vector &vx ALIEN_UNUSED_PARAM, Vector &vr
            ALIEN_UNUSED_PARAM) const {
        HYPRE_ParVectorAxpy(alpha, hypre_implem(vx), hypre_implem(vr));
    }

    void
    InternalLinearAlgebra::copy(const Vector &vx, Vector &vr) const {
        HYPRE_ParVectorCopy(hypre_implem(vx), hypre_implem(vr));
    }

    Arccore::Real
    InternalLinearAlgebra::dot(const Vector &vx, const Vector &vy) const {
        double dot_prod = 0;
        HYPRE_ParVectorInnerProd(hypre_implem(vx), hypre_implem(vy), &dot_prod);
        return static_cast<Arccore::Real>(dot_prod);
    }

    void
    InternalLinearAlgebra::diagonal(Matrix const &m ALIEN_UNUSED_PARAM, Vector &v
                                    ALIEN_UNUSED_PARAM) const {
        throw Arccore::NotImplementedException(
                A_FUNCINFO, "HypreLinearAlgebra::diagonal not implemented");
    }

    void
    InternalLinearAlgebra::reciprocal(Vector &v ALIEN_UNUSED_PARAM) const {
        throw Arccore::NotImplementedException(
                A_FUNCINFO, "HypreLinearAlgebra::reciprocal not implemented");
    }

    void
    InternalLinearAlgebra::aypx(
            const double &alpha ALIEN_UNUSED_PARAM, Vector &y ALIEN_UNUSED_PARAM, const Vector &x
            ALIEN_UNUSED_PARAM) const {
        throw Arccore::NotImplementedException(A_FUNCINFO, "HypreLinearAlgebra::aypx not implemented");
    }

    void
    InternalLinearAlgebra::pointwiseMult(
            const Vector &x ALIEN_UNUSED_PARAM, const Vector &y ALIEN_UNUSED_PARAM, Vector &w
            ALIEN_UNUSED_PARAM) const {
        throw Arccore::NotImplementedException(
                A_FUNCINFO, "HypreLinearAlgebra::pointwiseMult not implemented");
    }

    void
    InternalLinearAlgebra::scal(const Arccore::Real &alpha, Vector &x) const {
        HYPRE_ParVectorScale(static_cast<double>(alpha), hypre_implem(x));
    }

    ALIEN_HYPRE_EXPORT
    IInternalLinearAlgebra<Hypre::Matrix, Hypre::Vector> *
    InternalLinearAlgebraFactory() {
        return new Hypre::InternalLinearAlgebra();
    }
}
