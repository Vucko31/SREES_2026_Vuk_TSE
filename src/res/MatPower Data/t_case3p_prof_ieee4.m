function mpc = t_case3p_prof_ieee4
% t_case3p_prof_ieee4 - IEEE 4-bus benchmark adapted from Dzafic DSSE book.
% Reference measurements from Table 6.1:
% |V2| = 0.9877 0.9921 0.9895
% |V3| = 0.9368 0.9451 0.9397
% |V4| = 0.8061 0.8635 0.8311
% Load per phase: PL = 0.9 p.u., QL = 0.4359 p.u. on 6000 kVA base.

%% MATPOWER Case Format : Version 2
mpc.version = '2';

%%-----  Power Flow Data  -----%%
mpc.baseMVA = 100;
mpc.bus = [];
mpc.gen = [];
mpc.branch = [];
mpc.gencost = [];

%%-----  3 Phase Model Data  -----%%
mpc.freq = 60;
mpc.basekVA = 6000;

%% 3-phase bus data
%	busid	type	basekV	Vm1	Vm2	Vm3	Va1	Va2	Va3
mpc.bus3p = [
	1	3	12.47	1	1	1	0	-120	120;
	2	1	12.47	1	1	1	0	-120	120;
	3	1	4.16	1	1	1	0	-120	120;
	4	1	4.16	1	1	1	0	-120	120;
];

%% buslink data
mpc.buslink = [];

%% 3-phase line data
%	brid	fbus	tbus	status	lcid	len
mpc.line3p = [
	1	1	2	1	1	2000/5280;
	2	3	4	1	1	2500/5280;
];

%% 3-phase transformer data
%	xfid	fbus	tbus	status	R	X	basekVA	basekV	ratio
mpc.xfmr3p = [
	1	2	3	1	0.01	0.06	6000	12.47	1;
];

%% 3-phase shunt data
%	shid	shbus	status	gs1	gs2	gs3	bs1	bs2	bs3
mpc.shunt3p = [];

%% 3-phase load data
%	ldid	ldbus	status	Pd1	Pd2	Pd3	ldpf1	ldpf2	ldpf3
% 0.9 p.u. on 6000 kVA = 5400 kW per phase.
% Q/P = 0.4359/0.9 gives pf approximately 0.9 lagging.
mpc.load3p = [
	1	4	1	5400	5400	5400	0.9	0.9	0.9;
];

%% 3-phase generator data
%	genid	gbus	status	Vg1	Vg2	Vg3	Pg1	Pg2	Pg3	Qg1	Qg2	Qg3
mpc.gen3p = [];

%% line construction data
%	lcid	R11	R21	R31	R22	R32	R33	X11	X21	X31	X22	X32	X33	C11	C21	C31	C22	C32	C33
% R/X values are ohm per mile; shunt capacitances are set to zero because
% the book example states shunt admittances are not considered.
mpc.lc = [
	1	0.403	0.128	0.126	0.410	0.130	0.406	1.063	0.486	0.370	1.032	0.408	1.050	0	0	0	0	0	0;
];
