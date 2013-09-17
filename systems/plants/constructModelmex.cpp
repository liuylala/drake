#include "mex.h"
#include <Eigen/Dense>
#include <vector>
#include <iostream>
#include "drakeUtil.h"
#include "RigidBodyManipulator.h"

#define INF -2147483648

using namespace Eigen;
using namespace std;

// convert Matlab cell array of strings into a C++ vector of strings
vector<string> get_strings(const mxArray *rhs) {
  int num = mxGetNumberOfElements(rhs);
  vector<string> strings(num);
  for (int i=0; i<num; i++) {
    const mxArray *ptr = mxGetCell(rhs,i);
    int buflen = mxGetN(ptr)*sizeof(mxChar)+1;
    char* str = (char*)mxMalloc(buflen);
    mxGetString(ptr, str, buflen);
    strings[i] = string(str);
    mxFree(str);
  }
  return strings;
}



void mexFunction( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
  //DEBUG
  //cout << "constructModelmex: START" << endl;
  //END_DEBUG
  char buf[100];
  mxArray *pm;

  if (nrhs!=1) {
    mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","Usage model_ptr = constructModelmex(obj)");
  }

  RigidBodyManipulator *model=NULL;

//  model->robot_name = get_strings(mxGetProperty(prhs[0],0,"name"));

  const mxArray* featherstone = mxGetProperty(prhs[0],0,"featherstone");
  if (!featherstone) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs", "the featherstone array is invalid");

  const mxArray* pBodies = mxGetProperty(prhs[0],0,"body");
  if (!pBodies) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","the body array is invalid");
  int num_bodies = mxGetNumberOfElements(pBodies);

  // set up the model
  int dim=3;
  
  pm = mxGetField(featherstone,0,"NB");
  if (!pm) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.NB.  Are you passing in the correct structure?");
  model = new RigidBodyManipulator((int) mxGetScalar(pm), (int) mxGetScalar(pm), num_bodies);
  
  pm = mxGetField(featherstone,0,"parent");
  if (!pm) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.parent.");
  double* parent = mxGetPr(pm);
  
  pm = mxGetField(featherstone,0,"pitch");
  if (!pm) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.pitch.");
  double* pitch = mxGetPr(pm);
  
  pm = mxGetField(featherstone,0,"dofnum");
  if (!pm) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.dofnum.");
  double* dofnum = mxGetPr(pm);

  pm = mxGetField(featherstone,0,"damping");
  if (!pm) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.damping.");
  memcpy(model->damping,mxGetPr(pm),sizeof(double)*model->NB);
  
  pm = mxGetField(featherstone,0,"coulomb_friction");
  if (!pm) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.coulomb_friction.");
  memcpy(model->coulomb_friction,mxGetPr(pm),sizeof(double)*model->NB);
  
  pm = mxGetField(featherstone,0,"static_friction");
  if (!pm) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.static_friction.");
  memcpy(model->static_friction,mxGetPr(pm),sizeof(double)*model->NB);
  
  pm = mxGetField(featherstone,0,"coulomb_window");
  if (!pm) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.coulomb_window.");
  memcpy(model->coulomb_window,mxGetPr(pm),sizeof(double)*model->NB);
  
  mxArray* pXtree = mxGetField(featherstone,0,"Xtree");
  if (!pXtree) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.Xtree.");
  
  mxArray* pI = mxGetField(featherstone,0,"I");
  if (!pI) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't find field model.featherstone.I.");
  
  for (int i=0; i<model->NB; i++) {
    model->parent[i] = ((int) parent[i]) - 1;  // since it will be used as a C index
    model->pitch[i] = (int) pitch[i];
    model->dofnum[i] = (int) dofnum[i] - 1; // zero-indexed

    mxArray* pXtreei = mxGetCell(pXtree,i);
    if (!pXtreei) mexErrMsgIdAndTxt("Drake:HandCpmex:BadInputs","can't access model.featherstone.Xtree{%d}",i);
    
    // todo: check that the size is 6x6
    memcpy(model->Xtree[i].data(),mxGetPr(pXtreei),sizeof(double)*6*6);
    
    mxArray* pIi = mxGetCell(pI,i);
    if (!pIi) mexErrMsgIdAndTxt("Drake:constructModelmex:BadInputs","can't access model.featherstone.I{%d}",i);
    
    // todo: check that the size is 6x6
    memcpy(model->I[i].data(),mxGetPr(pIi),sizeof(double)*6*6);
  }

  for (int i=0; i<model->num_bodies; i++) {
    //DEBUG
    //cout << "constructModelmex: body " << i << endl;
    //END_DEBUG
    pm = mxGetProperty(pBodies,i,"linkname");
    mxGetString(pm,buf,100);
    model->bodies[i].linkname.assign(buf,strlen(buf));

    pm = mxGetProperty(pBodies,i,"jointname");
    mxGetString(pm,buf,100);
    model->bodies[i].jointname.assign(buf,strlen(buf));

    pm = mxGetProperty(pBodies,i,"mass");
    model->bodies[i].mass = (double) mxGetScalar(pm);

    pm = mxGetProperty(pBodies,i,"contact_pts");
    if (!mxIsEmpty(pm)) {
      Map<MatrixXd> pts_tmp(mxGetPr(pm),mxGetM(pm),mxGetN(pm));
      model->bodies[i].contact_pts.resize(4,mxGetN(pm));
      model->bodies[i].contact_pts << pts_tmp, MatrixXd::Ones(1,mxGetN(pm));
    }
    
    pm = mxGetProperty(pBodies,i,"com");
    if (!mxIsEmpty(pm)) memcpy(model->bodies[i].com.data(),mxGetPr(pm),sizeof(double)*3);

    pm = mxGetProperty(pBodies,i,"dofnum");
    model->bodies[i].dofnum = (int) mxGetScalar(pm) - 1;  //zero-indexed
    
    pm = mxGetProperty(pBodies,i,"floating");
    model->bodies[i].floating = (int) mxGetScalar(pm);

    pm = mxGetProperty(pBodies,i,"pitch");
    model->bodies[i].pitch = (int) mxGetScalar(pm);
    
    pm = mxGetProperty(pBodies,i,"parent");
    if (!pm || mxIsEmpty(pm))
    	model->bodies[i].parent = -1;
    else
    	model->bodies[i].parent = mxGetScalar(pm) - 1;
    
//     if (model->bodies[i].dofnum>=0) {
//       pm = mxGetProperty(pBodies,i,"joint_limit_min");
//       model->joint_limit_min[model->bodies[i].dofnum] = mxGetScalar(pm);
//       pm = mxGetProperty(pBodies,i,"joint_limit_max");
//       model->joint_limit_max[model->bodies[i].dofnum] = mxGetScalar(pm);
//     }    
    
    pm = mxGetProperty(pBodies,i,"Ttree");
    // todo: check that the size is 4x4
    memcpy(model->bodies[i].Ttree.data(),mxGetPr(pm),sizeof(double)*4*4);
    
    pm = mxGetProperty(pBodies,i,"T_body_to_joint");
    memcpy(model->bodies[i].T_body_to_joint.data(),mxGetPr(pm),sizeof(double)*4*4);

    //DEBUG
    //cout << "constructModelmex: About to parse collision geometry"  << endl;
    //END_DEBUG
    pm = mxGetProperty(pBodies,i,"contact_shapes");
    Matrix4d T;
    if(!mxIsEmpty(pm)){
      for (int j=0; j<mxGetNumberOfElements(pm); j++) {
        //DEBUG
        //cout << "constructModelmex: Body " << i << ", Element " << j << endl;
        //END_DEBUG
        auto shape = (DrakeCollision::Shape)mxGetScalar(mxGetField(pm,j,"type"));
        vector<double> params_vec;
        double* params = mxGetPr(mxGetField(pm,j,"params"));
        int n_params = (int) mxGetNumberOfElements(mxGetField(pm,j,"params"));
        for (int k=0; k<n_params; k++) {
          params_vec.push_back(params[k]);
        }
        memcpy(T.data(), mxGetPr(mxGetField(pm,j,"T")), sizeof(double)*4*4);
        model->addCollisionElement(i,T,shape,params_vec);
        if (model->bodies[i].parent<0) {
          model->updateCollisionElements(i);  // update static objects only once - right here on load
        }
      }


      // Set collision filtering bitmasks
      pm = mxGetProperty(pBodies,i,"collision_filter");
      const uint16_t* group = (uint16_t*)mxGetPr(mxGetField(pm,0,"belongs_to"));
      const uint16_t* mask = (uint16_t*)mxGetPr(mxGetField(pm,0,"collides_with"));
      //DEBUG
      //cout << "constructModelmex: Group: " << *group << endl;
      //cout << "constructModelmex: Mask " << *mask << endl;
      //END_DEBUG
      model->setCollisionFilter(i,*group,*mask);
    }
  }
  
  double *min = mxGetPr(mxGetProperty(prhs[0],0,"joint_limit_min"));
  double *max = mxGetPr(mxGetProperty(prhs[0], 0, "joint_limit_max"));
  
  memcpy(model->joint_limit_min, mxGetPr(mxGetProperty(prhs[0],0,"joint_limit_min")), sizeof(double)*model->num_dof);
  memcpy(model->joint_limit_max, mxGetPr(mxGetProperty(prhs[0],0,"joint_limit_max")), sizeof(double)*model->num_dof);
  
  const mxArray* a_grav_array = mxGetProperty(prhs[0],0,"gravity");
  if (a_grav_array && mxGetNumberOfElements(a_grav_array)==3) {
    double* p = mxGetPr(a_grav_array);
    model->a_grav[3] = p[0];
    model->a_grav[4] = p[1];
    model->a_grav[5] = p[2];
  } else {
    mexErrMsgIdAndTxt("Drake:constructModelmex:BadGravity","Couldn't find a 3 element gravity vector in the object.");
  }

  model->compile();
  
  if (nlhs>0) {  // return a pointer to the model
  	plhs[0] = createDrakeMexPointer((void*)model,"deleteModelmex","RigidBodyManipulator");
  }

}
