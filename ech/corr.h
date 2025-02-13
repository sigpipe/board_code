

void corr_init(int plen_bits, int pr_pd_sammps);
void corr_accum(double corr[], short int buf[]);
void corr_find_peaks(double corr[], int num_probes);
