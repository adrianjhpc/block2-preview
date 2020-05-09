
#include "quantum.hpp"
#include <gtest/gtest.h>

using namespace block2;

class TestAncilla : public ::testing::Test {
  protected:
    size_t isize = 1L << 30;
    size_t dsize = 1L << 36;
    void SetUp() override {
        Random::rand_seed(0);
        frame = new DataFrame(isize, dsize, "nodex");
    }
    void TearDown() override {
        frame->activate(0);
        assert(ialloc->used == 0 && dalloc->used == 0);
        delete frame;
    }
};

TEST_F(TestAncilla, Test) {
    shared_ptr<FCIDUMP> fcidump = make_shared<FCIDUMP>();
    string filename = "data/HUBBARD-L8.FCIDUMP"; // E = -6.22563376
    fcidump->read(filename);
    vector<uint8_t> orbsym = fcidump->orb_sym();
    transform(orbsym.begin(), orbsym.end(), orbsym.begin(),
              Hamiltonian::swap_d2h);
    SpinLabel vaccum(0);
    SpinLabel target(fcidump->n_sites() * 2, fcidump->twos(),
                     Hamiltonian::swap_d2h(fcidump->isym()));
    int n_physical_sites = fcidump->n_sites();
    int n_sites = n_physical_sites * 2;
    bool su2 = !fcidump->uhf;
    Hamiltonian hamil(vaccum, target, n_physical_sites, su2, fcidump, orbsym);

    // Ancilla MPSInfo
    shared_ptr<AncillaMPSInfo> mps_info = make_shared<AncillaMPSInfo>(
        n_physical_sites, vaccum, target, hamil.basis, hamil.orb_sym, hamil.n_syms);
    mps_info->set_thermal_limit();
    cout << "left dims = ";
    for (int i = 0; i <= n_sites; i++)
        cout << mps_info->left_dims_fci[i].n_states_total << " ";
    cout << endl;
    cout << "right dims = ";
    for (int i = 0; i <= n_sites; i++)
        cout << mps_info->right_dims_fci[i].n_states_total << " ";
    cout << endl;

    // Ancilla MPS
    shared_ptr<MPS> mps = make_shared<MPS>(n_sites, 0, 2);
    mps->initialize(mps_info);
    mps->fill_thermal_limit();
    for (int i = 0; i < n_sites; i++)
        if (mps->tensors[i] != nullptr)
            cout << i << *mps->tensors[i]->info << endl << *mps->tensors[i] << endl;

    // MPS/MPSInfo save mutable
    mps->save_mutable();
    mps->deallocate();
    mps_info->save_mutable();
    mps_info->deallocate_mutable();

    Timer t;
    t.get_time();
    // MPO construction
    cout << "MPO start" << endl;
    shared_ptr<MPO> mpo = make_shared<QCMPO>(hamil, QCTypes::NC);
    cout << "MPO end .. T = " << t.get_time() << endl;

    // Ancilla MPO construction
    cout << "Ancilla MPO start" << endl;
    // mpo = make_shared<AncillaMPO>(mpo);
    cout << "Ancilla MPO end .. T = " << t.get_time() << endl;

    // MPO simplification
    cout << "MPO simplification start" << endl;
    mpo = make_shared<SimplifiedMPO>(mpo, make_shared<RuleQCSU2>());
    cout << "MPO simplification end .. T = " << t.get_time() << endl;
    cout << mpo->get_blocking_formulas() << endl;
    abort();

    // uint16_t bond_dim = 250;

    // MPSInfo
    // shared_ptr<MPSInfo> mps_info = make_shared<MPSInfo>(
    //     n_physical_sites, vaccum, target, hamil.basis, hamil.orb_sym, hamil.n_syms);
    // mps_info->set_bond_dimension(50);
    // assert(occs.size() == norb);
    // for (size_t i = 0; i < occs.size(); i++)
    //     cout << occs[i] << " ";
    // mps_info->set_bond_dimension_using_occ(bond_dim, occs);
    // cout << "left min dims = ";
    // for (int i = 0; i <= norb; i++)
    //     cout << mps_info->left_dims_fci[i].n << " ";
    // cout << endl;
    // cout << "right min dims = ";
    // for (int i = 0; i <= norb; i++)
    //     cout << mps_info->right_dims_fci[i].n << " ";
    // cout << endl;
    // cout << "left q dims = ";
    // for (int i = 0; i <= norb; i++)
    //     cout << mps_info->left_dims[i].n << " ";
    // cout << endl;
    // cout << "right q dims = ";
    // for (int i = 0; i <= norb; i++)
    //     cout << mps_info->right_dims[i].n << " ";
    // cout << endl;
    // cout << "left dims = ";
    // for (int i = 0; i <= n_sites; i++)
    //     cout << mps_info->left_dims[i].n_states_total << " ";
    // cout << endl;
    // cout << "right dims = ";
    // for (int i = 0; i <= n_sites; i++)
    //     cout << mps_info->right_dims[i].n_states_total << " ";
    // cout << endl;

    // // MPS
    // Random::rand_seed(0);
    // shared_ptr<MPS> mps = make_shared<MPS>(norb, 0, 2);
    // mps->initialize(mps_info);
    // mps->random_canonicalize();

    // // MPS/MPSInfo save mutable
    // mps->save_mutable();
    // mps->deallocate();
    // mps_info->save_mutable();
    // mps_info->deallocate_mutable();

    // frame->activate(0);
    // cout << "persistent memory used :: I = " << ialloc->used
    //      << " D = " << dalloc->used << endl;
    // frame->activate(1);
    // cout << "exclusive  memory used :: I = " << ialloc->used
    //      << " D = " << dalloc->used << endl;
    // // ME
    // shared_ptr<TensorFunctions> tf = make_shared<TensorFunctions>(hamil.opf);
    // hamil.opf->seq->mode = SeqTypes::Simple;
    // shared_ptr<MovingEnvironment> me =
    //     make_shared<MovingEnvironment>(mpo, mps, mps, tf, hamil.site_op_infos);
    // me->init_environments();

    // cout << *frame << endl;
    // frame->activate(0);

    // // DMRG
    // vector<uint16_t> bdims = {250, 250, 250, 250, 250, 500, 500, 500,
    //                           500, 500, 750, 750, 750, 750, 750};
    // vector<double> noises = {1E-6, 1E-6, 1E-6, 1E-6, 1E-6, 0.0};
    // // vector<uint16_t> bdims = {200};
    // // vector<double> noises = {0};
    // shared_ptr<DMRG> dmrg = make_shared<DMRG>(me, bdims, noises);
    // dmrg->solve(30, true);

    // deallocate persistent stack memory
    mpo->deallocate();
    mps_info->deallocate();
    hamil.deallocate();
    fcidump->deallocate();
}
