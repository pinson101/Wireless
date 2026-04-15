%% CSE4377 Lab 6 - Demodulation and Reception
%% =========================================================================
%  RTL-SDR data capture analysis 
%% Figures 
%  1. Raw signal in IQ (step 12)
%  2. Filtered signal in IQ (step 14)
%  3. CFO 
% =========================================================================

clear; close all; clc;

%% GLOBALS?

IQ_FILE       = 'file_name.iq';   % Path!!!
f_offset      = 0;                % LO frequency offset in Hz
%  for step 15, A positive f_offset shifts the spectrum LEFT.


% DEPENDS ON HOW WE DO PREAMBLE, we could use xcorr() like WiFi

% Preamble bit sequence
PREAMBLE_BITS = [0 1 1 1 1 1 0 0 0 0 0 1]; % avoids 4th quadrant 

% for finding BER, what are the known transmitted bits 
TX_DATA_BITS  = [];   % e.g., repmat([0 0 0 1 1 0 1 1], 1, 50)

%% STEP 12: Read IQ File

len  = 2048000;
file = fopen(IQ_FILE, 'rb');
if file == -1
    error('Cannot open file "%s". Check IQ_FILE path.', IQ_FILE);
end
rx = zeros(len, 1);
for i = 1:len
    re    = fread(file, 1, 'uint8=>double') - 128;
    im    = fread(file, 1, 'uint8=>double') - 128;
    rx(i) = complex(re, im);
end
fclose(file);
fprintf('  Read %d complex samples.\n', len);

% FIGURE 1: RAW on the IQ plane
figure('Name','Step 13 – Raw IQ Constellation','NumberTitle','off');
scatter(real(rx(1:10:end)), imag(rx(1:10:end)), 1, [0.2 0.45 0.8], '.');
axis equal; grid on;
title('Step 13: Raw IQ Constellation (1-of-10 samples shown)');
xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');

%% STEP 13: Design Decimating FIR Filter

Fs    = 2048000;          % Sampling Frequency (Hz)
Fpass = 8000;             % Passband edge (Hz)  — matches 8 ksps QPSK symbol rate
Fstop = 64000;            % Stopband edge (Hz)
Dpass = 0.057501127785;   % Passband ripple
Dstop = 0.0001;           % Stopband attenuation
dens  = 20;               % Density factor

[N, Fo, Ao, W] = firpmord([Fpass, Fstop]/(Fs/2), [1 0], [Dpass, Dstop]); 
% returns the order estimate N, normalized frequency band edges Fo, frequency band amplitudes Ao, and weights W that meet input specifications f, a, and dev
h8000 = firpm(N, Fo, Ao, W, {dens}); % vector with coef of FIR filter
fprintf('  Filter order: %d\n', N);

% Filter and decimate !!!!!
fprintf('  Filtering and decimating ...\n');
rxFilt = conv(rx, h8000); % convolutes two vectors
rxFilt = rxFilt(1 : Fs/Fpass : end);   % keep 1 of every 256 (8000 samples at 8 ksps)
fprintf('  Decimated to %d samples at %d ksps.\n', length(rxFilt), Fpass/1e3);

%% STEP 14: Plot Filtered Constellation (LO Offset Visible as Circle)

figure('Name','Step 14 – Filtered Constellation (LO Offset)','NumberTitle','off');
scatter(real(rxFilt), imag(rxFilt), 8, [0.85 0.2 0.2], '.');
axis equal; grid on;
title('Step 14: Filtered & Decimated IQ Constellation');
xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');

%% STEP 15: Frequency Offset Correction

idx_vec  = (0:len-1)';
rxCorr   = rx .* complex(cos(2*pi*idx_vec/Fs*f_offset), sin(2*pi*idx_vec/Fs*f_offset));
% mult filtered signal by rotating phase to get rid of CFO

% refilter 
rxCFilt  = conv(rxCorr, h8000);
rxCFilt  = rxCFilt(1 : Fs/Fpass : end);

figure('Name','Step 15 – Frequency-Corrected Constellation','NumberTitle','off');
scatter(real(rxCFilt), imag(rxCFilt), 8, [0.1 0.65 0.3], '.');
axis equal; grid on;
title('Step 15: Frequency-Corrected Constellation');
xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');

%% STEP 18: QPSK Constellation Check (before preamble alignment)
figure('Name','Step 18 – QPSK Constellation','NumberTitle','off');
scatter(real(rxCFilt), imag(rxCFilt), 8, [0.55 0.1 0.8], '.');
axis equal; grid on;
title('Step 18: QPSK Constellation After Frequency Correction');
xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');

%% STEP 19-20: Preamble Definition & Correlation for Timing Recovery
n_pre_bits = length(PREAMBLE_BITS);
n_pre_syms = n_pre_bits / 2;
preamble_syms = zeros(n_pre_syms, 1);

% basically makes a wave copy of what the preamble should look like
% Bit pairs: 00->+1+j  01->-1+j  11->-1-j  10->+1-j
for k = 1:n_pre_syms
    b0 = PREAMBLE_BITS(2*k-1);
    b1 = PREAMBLE_BITS(2*k);
    if     b0==0 && b1==0;  preamble_syms(k) =  1+1j;
    elseif b0==0 && b1==1;  preamble_syms(k) = -1+1j;
    elseif b0==1 && b1==1;  preamble_syms(k) = -1-1j;
    else;                   preamble_syms(k) =  1-1j;
    end
end
preamble_syms = preamble_syms / norm(preamble_syms);   % normalize

% XCORR and look for peak (peak is where it starts)
corr_full = xcorr(rxCFilt, preamble_syms);
% xcorr output length = 2*N-1; zero-lag at index N
N_rx    = length(rxCFilt);
N_pre   = length(preamble_syms);
corr_pos = abs(corr_full(N_pre : end));   % positive-lag portion

[~, peak_idx] = max(corr_pos);            % 1-based index into rxCFilt
fprintf('  Preamble peak at sample index %d  (of %d total at 8 ksps)\n', ...
        peak_idx, N_rx);

figure('Name','Step 20 – Preamble Correlation','NumberTitle','off');
plot((0:length(corr_pos)-1), corr_pos, 'Color', [0.2 0.5 0.8], 'LineWidth', 0.8);
hold on;
plot(peak_idx-1, corr_pos(peak_idx), 'r*', 'MarkerSize', 12, 'LineWidth', 2);
hold off;
grid on;
title('Step 20: Preamble Correlation Output');
xlabel('Sample Index (at 8 ksps)'); ylabel('|Correlation|');
legend('Correlation Magnitude', sprintf('Peak at index %d', peak_idx-1), ...
       'Location','best');

%% STEP 21: Phase Rotation Correction, Demodulation, and BER
data_start = peak_idx + N_pre;   % data starts at preamble start + preamble length

% find phase rotation 
rx_pre_window = rxCFilt(peak_idx : peak_idx + N_pre - 1);
phase_est     = angle(sum(rx_pre_window .* conj(preamble_syms)));
fprintf('  Estimated constellation rotation: %.2f deg\n', rad2deg(phase_est));

% get data
n_data = N_rx - data_start;   % remaining symbols after preamble
rx_data_raw  = rxCFilt(data_start : data_start + n_data - 1);
rx_data_corr = rx_data_raw * exp(-1j * phase_est);

% QPSK decisions 
rx_bits = zeros(n_data * 2, 1);
for k = 1:n_data
    s = rx_data_corr(k);
    I = real(s);  Q = imag(s);
    if     I >= 0 && Q >= 0;  rx_bits(2*k-1:2*k) = [0; 0];
    elseif I <  0 && Q >= 0;  rx_bits(2*k-1:2*k) = [0; 1];
    elseif I <  0 && Q <  0;  rx_bits(2*k-1:2*k) = [1; 1];
    else;                      rx_bits(2*k-1:2*k) = [1; 0];
    end
end

fprintf('64 QPSK bits:\n    ');
fprintf('%d', rx_bits(1:min(64,end)));
fprintf('\n');

% --- BER (if TX bits are provided) ---
if ~isempty(TX_DATA_BITS)
    tx = TX_DATA_BITS(:);
    n_compare = min(length(tx), length(rx_bits));
    n_err = sum(rx_bits(1:n_compare) ~= tx(1:n_compare));
    BER   = n_err / n_compare;
    fprintf('  BER: %d errors over %d bits = %.6f\n', n_err, n_compare, BER);
    ber_str = sprintf('BER = %.4f  (%d / %d bits)', BER, n_err, n_compare);
else
    ber_str = 'BER: TX_DATA_BITS not set';
    fprintf('  %s\n', ber_str);
end

figure('Name','Step 21 – Demodulated QPSK Constellation','NumberTitle','off');
scatter(real(rx_data_corr), imag(rx_data_corr), 6, [0.15 0.55 0.9], '.');
hold on;
% Decision boundary lines
xline(0, 'k--', 'LineWidth', 1.5);
yline(0, 'k--', 'LineWidth', 1.5);
% Ideal QPSK points scaled to mean received amplitude
amp = mean(abs(rx_data_corr));
ideal = amp * [1+1j, -1+1j, -1-1j, 1-1j];
plot(real(ideal), imag(ideal), 'r+', 'MarkerSize', 18, 'LineWidth', 2.5);
hold off;
axis equal; grid on;
title({'Step 21: Rotation-Corrected QPSK Constellation', ber_str});
xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');
legend('Received Symbols', 'Decision Boundaries', 'Ideal QPSK Points', ...
       'Location','best');