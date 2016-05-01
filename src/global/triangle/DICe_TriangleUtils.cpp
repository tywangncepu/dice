// @HEADER
// ************************************************************************
//
//               Digital Image Correlation Engine (DICe)
//                 Copyright 2015 Sandia Corporation.
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
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
// Questions? Contact: Dan Turner (dzturne@sandia.gov)
//
// ************************************************************************
// @HEADER

#include <DICe_TriangleUtils.h>
#include <DICe_MeshIO.h>

#include <triangle.h>

namespace DICe {

Teuchos::RCP<DICe::mesh::Mesh> generate_tri6_mesh(Teuchos::ArrayRCP<scalar_t> points_x,
  Teuchos::ArrayRCP<scalar_t> points_y,
  const scalar_t & max_size_constraint,
  const std::string & output_file_name){

  TEUCHOS_TEST_FOR_EXCEPTION(points_x.size()<=0||points_y.size()<=0,std::runtime_error,"Error, points size must be greater than 0");
  TEUCHOS_TEST_FOR_EXCEPTION(points_x.size()!=points_y.size(),std::runtime_error,"Error, points must be the same size");
  TEUCHOS_TEST_FOR_EXCEPTION(output_file_name=="",std::runtime_error,"Error, ivalid output file name");

  // set up the traingle data structures

  struct triangulateio in, out;
  in.numberofpoints = points_x.size();
  in.numberofpointattributes = 0;
  in.pointmarkerlist = (int *) NULL;
  in.pointlist = new REAL[in.numberofpoints*2];
  for(int_t i=0;i<points_x.size();++i){
    in.pointlist[i*2+0] = points_x[i];
    in.pointlist[i*2+1] = points_y[i];
  }
  in.numberofsegments = 0;
  in.numberofholes = 0;
  in.numberofregions = 0;
  out.pointlist = (REAL *) NULL;
  out.trianglelist = (int *) NULL;
  out.triangleattributelist = (REAL *) NULL;
  out.pointmarkerlist = (int *) NULL;
  out.segmentlist = (int *) NULL;
  out.segmentmarkerlist = (int *) NULL;
  out.edgelist = (int *) NULL;
  out.edgemarkerlist = (int *) NULL;

  // use the triangle library to generate the triangle elements

  // using the o2 flag here to get quadratic tris with 6 nodes each
  std::stringstream arg_ss;
  arg_ss << "qpcAeo2a" << max_size_constraint;
  char args[1024];
  strncpy(args, arg_ss.str().c_str(), sizeof(args));
  args[sizeof(args) - 1] = 0;
  DEBUG_MSG("generate_tri6_mesh() called with args: " << args);
//  char args[] = arg_ss.str().c_str();
  triangulate(args,&in,&out,NULL);

  DEBUG_MSG("number of boundary segments: " << out.numberofsegments);
  std::set<int_t> boundary_nodes;
  //std::cout << "segments: " << std::endl;
  for(int_t i=0;i<out.numberofsegments;++i){
    //std::cout << i << " left index " << out.segmentlist[i*2+0] << " right " << out.segmentlist[i*2+1] << std::endl;
    boundary_nodes.insert(out.segmentlist[i*2+0]);
    boundary_nodes.insert(out.segmentlist[i*2+1]);
  }
  DEBUG_MSG("number of triangle edges: " << out.numberofedges);
  //std::cout << "edges: " << std::endl;
  //for(int_t i=0;i<out.numberofedges;++i){
  //  std::cout << i << " left index " << out.edgelist[i*2+0] << " right " << out.edgelist[i*2+1] << std::endl;
  //}

  // convert the resulting mesh to an exodus mesh


  Teuchos::ArrayRCP<int_t> connectivity(out.numberoftriangles*out.numberofcorners); // numberofcorners is num nodes per elem
  for(int_t i=0;i<out.numberoftriangles;++i){
    for (int_t j = 0; j < out.numberofcorners; j++) {
      connectivity[i*out.numberofcorners + j] = out.trianglelist[i * out.numberofcorners + j];
      // search the connectivity for edge elements and side sets
      for(int_t k=0;k<out.numberofsegments;++k){
        if(out.segmentlist[k*2] == out.trianglelist[i*out.numberofcorners + j]){
          //std::cout << "segment " << k << " left is in element " << i  << " position " << j << std::endl;
          for(int_t m=0;m<out.numberofcorners;++m){
            if(out.segmentlist[k*2+1] == out.trianglelist[i*out.numberofcorners +m]){
              //std::cout << "segment " << k << " right is in element " << i  << " position " << m << std::endl;
              // now that the element and side are known for boundary nodes add the middle node
              int_t third_node_pos = -1;
              if((j==0&&m==1)||(j==1&&m==0)){
                third_node_pos = 5;
              }
              else if((j==1&&m==2)||(j==2&&m==1)){
                third_node_pos = 3;
              }
              else if((j==0&&m==2)||(j==2&&m==0)){
                third_node_pos = 4;
              }
              else{
                TEUCHOS_TEST_FOR_EXCEPTION(true,std::runtime_error,"Error, invalid segment found");
              }
              boundary_nodes.insert(out.trianglelist[i*out.numberofcorners + third_node_pos]);
              break;
            }
          } // right end of segment
          break;
        } // left end of segment
      }

    }
  }

  DEBUG_MSG("boundary nodes");
  std::set<int_t>::const_iterator it = boundary_nodes.begin();
  std::set<int_t>::const_iterator it_end = boundary_nodes.end();
  for(;it!=it_end;++it){
    DEBUG_MSG(*it);
  }

  Teuchos::ArrayRCP<scalar_t> node_coords_x(out.numberofpoints); // numberofcorners is num nodes per elem
  Teuchos::ArrayRCP<scalar_t> node_coords_y(out.numberofpoints); // numberofcorners is num nodes per elem
  for(int_t i=0;i<out.numberofpoints;++i){
    node_coords_x[i] = out.pointlist[i*2+0];
    node_coords_y[i] = out.pointlist[i*2+1];
  }
  Teuchos::RCP<DICe::mesh::Mesh> mesh =
      DICe::mesh::create_tri6_exodus_mesh(node_coords_x,node_coords_y,connectivity,boundary_nodes,output_file_name);

  delete[] in.pointlist;
  delete[] out.pointlist;
  delete[] out.pointmarkerlist;
  delete[] out.trianglelist;
  delete[] out.triangleattributelist;
  delete[] out.segmentlist;
  delete[] out.segmentmarkerlist;
  delete[] out.edgelist;
  delete[] out.edgemarkerlist;

  return mesh;
}


}// End DICe Namespace
