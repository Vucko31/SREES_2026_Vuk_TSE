function mpc = t_case3p_stress
% t_case3p_stress - Unbalanced 5-bus radial 3-phase stress test.
% This case is intentionally different from t_case3p_a/b/c/d/e:
% longer feeders, heavier unbalanced loads, and no transformer section.

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
mpc.basekVA = 1000;

%% 3-phase bus data
%	busid	type	basekV	Vm1	Vm2	Vm3	Va1	Va2	Va3
mpc.bus3p = [
	1	3	12.47	1.00	1.00	1.00	0	-120	120;
	2	1	12.47	0.99	0.985	0.98	-1	-121	119;
	3	1	12.47	0.98	0.970	0.960	-2	-122	118;
	4	1	12.47	0.97	0.955	0.940	-3	-123	117;
	5	1	12.47	0.96	0.940	0.920	-4	-124	116;
];

%% buslink data
mpc.buslink = [];

%% 3-phase line data
%	brid	fbus	tbus	status	lcid	len
mpc.line3p = [
	1	1	2	1	1	1.60;
	2	2	3	1	1	1.35;
	3	3	4	1	1	1.10;
	4	4	5	1	1	0.90;
];

%% 3-phase transformer data
%	xfid	fbus	tbus	status	R	X	basekVA	basekV	ratio
mpc.xfmr3p = [];

%% 3-phase shunt data
%	shid	shbus	status	gs1	gs2	gs3	bs1	bs2	bs3
mpc.shunt3p = [
	1	5	1	0	0	0	20	10	5;
];

%% 3-phase load data
%	ldid	ldbus	status	Pd1	Pd2	Pd3	ldpf1	ldpf2	ldpf3
mpc.load3p = [
	1	3	1	450	700	950	0.88	0.84	0.80;
	2	4	1	800	1100	1450	0.86	0.82	0.78;
	3	5	1	1200	1650	2200	0.84	0.80	0.76;
];

%% 3-phase generator data
%	genid	gbus	status	Vg1	Vg2	Vg3	Pg1	Pg2	Pg3	Qg1	Qg2	Qg3
mpc.gen3p = [
	1	1	1	1	1	1	6000	6000	6000	0	0	0;
];

%% line construction data
%	lcid	R11	R21	R31	R22	R32	R33	X11	X21	X31	X22	X32	X33	C11	C21	C31	C22	C32	C33
mpc.lc = [
	1	0.95	0.28	0.24	1.05	0.30	1.15	1.80	0.72	0.58	1.95	0.66	2.10	18.0	-5.4	-2.2	19.0	-3.8	17.5;
];
