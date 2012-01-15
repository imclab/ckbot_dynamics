function inds = get_block_indicies(size, i, step)
%  Returns the indicies for the ith diagonal 
%   block of size 6x6 from matrix of size
%   N x M x R (size = [N, M, R])
%
%  ARGUMENTS
%   size - Size of array to give indicies for (CURRENTLY UNUSED)
%   i - 6x6 block diagonal to extract indicies for
%   step - third dimension index
%
%  NOTES: Right now, all this does is return the range of rows and
%         columns needed
%
inds = (6*i-5):(6*i);

end