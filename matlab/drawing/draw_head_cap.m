function success = draw_head_cap(r_cg, R)
%  Draws A UBar solid body
%
% ARGUMENTS:
%  r_cg - 3x1 - Vector the the center of mass in the world frame
%  R - 3x3 - Rotation matrix that goes from the rigid body frame to the
%            world frame.
% RETURNS:
%  success - 1 for success, 0 for failure
success = 0;

% Rotation from definition coordinates to the correct coords (z-axis
% through forward joint)
R_correction = rotZ(pi/2)*rotX(pi/2);

% Load the body parameters from our centralized source
props = get_body_params('HT_head_cap');

% geometric parameters
s = props.width;
r_cg_ref = -props.r_back;
rad_tail = props.tail_radius;
h_tail = props.tail_len;
w_tail = props.tail_width;

% Drawing options
transparency = props.transparency;
face_color = props.face_color;
edge_color = props.edge_color;

%%% Defines all of the verticies needed to define the patch polygons
%   These are all in the body frame
%   These are also, at first, wrt a geometric point on the modules.
%   Then they're shifted to be wrt the center of mass, because the center
%   of mass location will likely change (while the geometry we wish to draw
%   won't)
verticies = [
    s/2,  s/2, s/3;    % 1
    -s/2, s/2, s/3;   % 2
    -s/2, -s/2, s/3;  % 3
    s/2, -s/2, s/3;   % 4
    s/2, -s/4, s/3;   % 5
    s/2, 0, s/3;      % 6
    -s/2, 0, s/3;     % 7
    -s/2, -s/4, s/3;  % 8
    w_tail/2, -s/4, h_tail; % 9
    w_tail/2, 0, h_tail;    % 10
    -w_tail/2, 0, h_tail;   % 11
    -w_tail/2, -s/4, h_tail;% 12
    rad_tail*cos(pi/4), -s/4, h_tail + rad_tail*sin(pi/4); % 13
    rad_tail*cos(pi/4), 0, h_tail + rad_tail*sin(pi/4); % 14
    0, 0, h_tail+rad_tail;     % 15
    0, -s/4, h_tail+rad_tail;  % 16
    rad_tail*cos(5*pi/4), 0, h_tail - rad_tail*sin(5*pi/4); % 17
    rad_tail*cos(5*pi/4), -s/4, h_tail - rad_tail*sin(5*pi/4); % 18
    -s/2, -s/2, 0; % 19
    -s/2*cos(pi/4), -s/2, -s/2*sin(pi/4);  % 20
    0, -s/2, -s/2; % 21
    -s/2*cos(5*pi/4), -s/2, s/2*sin(5*pi/4); % 22
    s/2, -s/2, 0; % 23
    s/2, s/2, 0; % 24
    s/2*cos(pi/4), s/2, -s/2*sin(pi/4); % 25
    0, s/2, -s/2; % 26
    s/2*cos(5*pi/4), s/2, s/2*sin(5*pi/4); % 27
    -s/2, s/2, 0; % 28
    -w_tail/2, -s/4, s/3; %29
    w_tail/2, -s/4, s/3; % 30
    -w_tail/2, 0, s/3; % 31
    w_tail/2, 0, s/3; % 32
    ];

verticies_inertial = zeros(size(verticies,1), 3);
for i=1:size(verticies,1)
   verticies_inertial(i,:) = r_cg' + (R*r_cg_ref)' + (R*R_correction*( verticies(i,:)'))';
end


patch_faces = [
 1,2,7,6,1,NaN,NaN;
 6,7,8,5,6,NaN,NaN;
 5,8,3,4,5,NaN,NaN;
 3,19,20,21,22,23,4;
 1,24,25,26,27,28,2;
];


head_cap_patch = patch('Vertices', verticies_inertial, 'Faces', patch_faces);
set(head_cap_patch, 'FaceAlpha', transparency);
set(head_cap_patch, 'FaceColor', face_color);
set(head_cap_patch, 'EdgeColor', edge_color);
success = 1;
end