%DILATION   Grey-value dilation
%
% SYNOPSIS:
%  image_out = dilation(image_in,filterSize,filterShape,boundary_condition)
%  image_out = dilation(image_in,image_se,boundary_condition)
%
% PARAMETERS:
%  filterSize:  sizes of the filter along each image dimension
%  filterShape: 'rectangular', 'elliptic', 'diamond', 'parabolic'
%  image_se:    binary or grey-value image with the shape for the structuring element
%  boundary_condition: Defines how the boundary of the image is handled.
%                      See HELP BOUNDARY_CONDITION
%
% DEFAULTS:
%  filterSize = 7
%  filterShape = 'elliptic'
%  boundary_condition = {} (equivalent to 'add min')
%
%  The structuring element can be specified in two ways: through FILTERSIZE
%  and FILTERSHAPE, specifying one of the default shapes, or through IMAGE_SE,
%  providing a custom binary or grey-value shape.
%
%  The IMAGE_SE is applied as-is as a neighborhood, without mirroring. Therefore,
%  the composition of DILATION and EROSION only forms an opening or closing if
%  the structuring element is symmetric. For non-symmetric structuring elements,
%  mirror the structuring element in one of the two operations.
%
% DIPlib:
%  This function calls the DIPlib function dip::Dilation.

% (c)2017, Cris Luengo.
% Based on original DIPlib code: (c)1995-2014, Delft University of Technology.
% Based on original DIPimage code: (c)1999-2014, Delft University of Technology.
%
% Licensed under the Apache License, Version 2.0 (the "License");
% you may not use this file except in compliance with the License.
% You may obtain a copy of the License at
%
%    http://www.apache.org/licenses/LICENSE-2.0
%
% Unless required by applicable law or agreed to in writing, software
% distributed under the License is distributed on an "AS IS" BASIS,
% WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
% See the License for the specific language governing permissions and
% limitations under the License.
