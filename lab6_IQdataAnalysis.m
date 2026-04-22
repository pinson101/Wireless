%% CSE4377 Lab 6 - Demodulation and Reception
%% =========================================================================
%  RTL-SDR data capture analysis 
%% Figures
% =========================================================================

clear; close all; clc;

IQ_FILE       = '/Users/angelina/Desktop/Wireless/AJJ_Lab06_Test4_QPSKpt2.iq';   % Path!!!

% Preamble bit sequence
PREAMBLE_BITS = [0 1 1 1 0 0 1 1 0 1 0 0 0 0 1 1]; % avoids 4th quadrant ASCII: "sC"

% for finding BER: what data bits are we transmitting, ASCII: "rtOH"
TX_DATA_BITS  = repmat([0 1 1 1 0 0 1 0 0 1 1 1 0 1 0 0 0 1 0 0 1 1 1 1 0 1 0 0 1 0 0 0], 1, 8);

%% STEP 12: Read IQ File

len  = 2048000;
file = fopen(IQ_FILE, 'rb');
if file == -1
    error('Cannot open file "%s". Check IQ_FILE path.', IQ_FILE);
end
rx = zeros(len, 1);
for i = 1:len
    re    = (fread(file, 1, "uint8=>double") - 128);
    im    = (fread(file, 1, "uint8=>double") - 128);
    rx(i) = complex(re, im);
end
fclose(file);
fprintf('  Read %d complex samples.\n', len);
rx

% FIGURE 1: RAW on the IQ plane
figure('Name','Step 13 – Raw IQ Constellation','NumberTitle','off');
plot(real(rx), imag(rx), '.');
% scatter(real(rx), imag(rx), '.');
axis equal; grid on;
title('Step 13: Raw IQ Constellation');
xlabel('I'); ylabel('Q');

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
rxFilt = rxFilt(1 : Fs/Fpass : end);   % keep 1 of every 256
fprintf('  Decimated to %d samples at %d ksps.\n', length(rxFilt), Fpass/1e3);

Fs_dec = Fpass;
N_dec  = length(rxFilt);
t_dec  = (0:N_dec-1)';

%% STEP 14: Plot Filtered Constellation (LO Offset Visible as Circle)

figure('Name','Step 14 – Filtered Constellation','NumberTitle','off');
scatter(real(rxFilt), imag(rxFilt), '.');
axis equal; grid on;
title('Step 14: Filtered & Decimated IQ Constellation');
xlabel('I'); ylabel('Q');

%% STEP 15: Frequency Offset Correction
% use fft to find estimate freq off then go through frequencies around it
% to find the best one 
fft_rxFilt = abs(fft(rxFilt));
figure(15);
plot(fft_rxFilt);
title("Complex Magnitude of fft Spectrum")
xlabel("f (Hz)")
ylabel("|fft(X)|")

% find frequency index of the peak
[~, best_f_idx] = max(fft_rxFilt(1:floor(N/2)));
best_f = ((best_f_idx - 1) * Fs_dec / length(rxFilt))/4;
fprintf('  Estimated frequency offset: %.2f Hz\n', best_f);

t_full = (0:len-1)';     % full-rate sample index vector


%%======= HERE


offset_estimate = 630;

% -------------------------------------------------------------------------
% 2.  Define sweep range
% -------------------------------------------------------------------------
sweep_half  = 10;     % ± Hz around estimate
sweep_step  = 0.1;    % Hz per frame
frame_pause = 0.04;   % seconds between frames (filtering is the bottleneck anyway)

f_values = offset_estimate - sweep_half : sweep_step : offset_estimate + sweep_half;
n_frames = length(f_values);

fprintf('\nSweeping %d frames from %.2f to %.2f Hz.\n', ...
        n_frames, f_values(1), f_values(end));
fprintf('Each frame runs full conv() + decimation on the raw rx signal.\n\n');

% -------------------------------------------------------------------------
% 3.  Animation figure
% -------------------------------------------------------------------------
fig_anim = figure('Name','Step 15 – Animated Offset Sweep (Full Filter)', ...
                  'NumberTitle','off','Position',[150 150 620 620],'Color','w');

ax = axes('Parent', fig_anim, 'Position', [0.12 0.20 0.76 0.70]);
axis(ax, 'equal');  grid(ax, 'on');
ylabel(ax, 'Quadrature (Q)');

% Compute first frame to initialise scatter handle
rxCorr_init  = rx .* complex(cos(2*pi*t_full/Fs*-f_values(1)), ...
                               sin(2*pi*t_full/Fs*-f_values(1)));
rxCFilt_init = conv(rxCorr_init, h8000);
rxCFilt_init = rxCFilt_init(1 : Fs/Fpass : end);

sc = scatter(ax, real(rxCFilt_init), imag(rxCFilt_init), 4, ...
             [0.1 0.65 0.3], '.', 'MarkerEdgeAlpha', 0.4);

txt_metric = text(ax, 0.01, 0.97, '', 'Units','normalized', ...
                  'FontSize', 9, 'Color',[0.5 0.5 0.5], ...
                  'VerticalAlignment','top');

best_f     = f_values(1);
best_score = Inf;

% -------------------------------------------------------------------------
% 4.  Sweep loop — full CFO + conv + decimate every frame
% -------------------------------------------------------------------------
for k = 1:n_frames

    if ~ishandle(fig_anim),  break;  end

    f_try = f_values(k);

    % --- CFO correction on full raw signal ---
    rxCorr = rx .* complex(cos(2*pi*t_full/Fs*-f_try), ...
                            sin(2*pi*t_full/Fs*-f_try));

    % --- Full FIR filter + decimate ---
    rxCFilt_frame = conv(rxCorr, h8000);
    rxCFilt_frame = rxCFilt_frame(1 : Fs/Fpass : end);

    % --- Cluster tightness metric (QPSK 4-fold symmetry) ---
    ang_var = var(mod(angle(rxCFilt_frame) * 4, 2*pi));
    if ang_var < best_score
        best_score = ang_var;
        best_f     = f_try;
    end

    % --- Update plot ---
    sc.XData = real(rxCFilt_frame);
    sc.YData = imag(rxCFilt_frame);

    r_max = max(abs(rxCFilt_frame)) * 1.15;
    axis(ax, [-r_max r_max -r_max r_max]);

    title(ax, sprintf('Step 15: f_{offset} = %.2f Hz          (best so far: %.2f Hz)', ...
                      f_try, best_f), 'FontSize', 11);
    txt_metric.String = sprintf('Cluster tightness score: %.4f  (lower = better)', ang_var);

    pct = k / n_frames * 100;
    xlabel(ax, sprintf('In-Phase (I)          [Sweep progress: %d%%  —  frame %d / %d]', ...
                       round(pct), k, n_frames));

    drawnow limitrate
    pause(frame_pause);
end

% -------------------------------------------------------------------------
% 5.  After sweep: freeze on best offset, add slider
% -------------------------------------------------------------------------
if ishandle(fig_anim)

    % Recompute best frame using full pipeline
    rxCorr_best  = rx .* complex(cos(2*pi*t_full/Fs*-best_f), ...
                                  sin(2*pi*t_full/Fs*-best_f));
    rxCFilt_best = conv(rxCorr_best, h8000);
    rxCFilt_best = rxCFilt_best(1 : Fs/Fpass : end);

    sc.XData = real(rxCFilt_best);
    sc.YData = imag(rxCFilt_best);
    r_max = max(abs(rxCFilt_best)) * 1.15;
    axis(ax, [-r_max r_max -r_max r_max]);
    title(ax, sprintf('Step 15: Best f_{offset} = %.2f Hz  (auto-selected)', best_f), ...
          'FontSize', 12, 'Color',[0.1 0.55 0.1]);
    xlabel(ax, 'In-Phase (I)');
    txt_metric.String = '';

    fprintf('Auto-selected best f_offset = %.3f Hz\n', best_f);

    % --- Slider ---
    uicontrol(fig_anim, 'Style','text', ...
              'Units','normalized', 'Position',[0.08 0.06 0.50 0.05], ...
              'String','Fine-tune offset (Hz):', ...
              'HorizontalAlignment','left', 'BackgroundColor','w', 'FontSize',9);

    slider_val_txt = uicontrol(fig_anim, 'Style','text', ...
              'Units','normalized', 'Position',[0.60 0.06 0.30 0.05], ...
              'String', sprintf('%.2f Hz', best_f), ...
              'HorizontalAlignment','center', 'BackgroundColor','w', ...
              'FontSize', 10, 'FontWeight','bold');

    uicontrol(fig_anim, 'Style','slider', ...
              'Units','normalized', 'Position',[0.08 0.01 0.84 0.045], ...
              'Min', offset_estimate - sweep_half, ...
              'Max', offset_estimate + sweep_half, ...
              'Value', best_f, ...
              'SliderStep', [sweep_step/(2*sweep_half), 5*sweep_step/(2*sweep_half)], ...
              'Callback', @(src,~) update_slider_full(src, rx, t_full, Fs, h8000, ...
                                                      Fpass, ax, sc, slider_val_txt));

    uicontrol(fig_anim, 'Style','pushbutton', ...
              'Units','normalized', 'Position',[0.60 0.12 0.30 0.05], ...
              'String','Copy offset to workspace', 'FontSize', 9, ...
              'Callback', @(~,~) assignin('base','f_offset_chosen', ...
                                           str2double(get(slider_val_txt,'String'))));
end

% -------------------------------------------------------------------------
% 6.  Apply chosen offset — full pipeline — to produce rxCFilt
% -------------------------------------------------------------------------
f_offset = best_f;
rxCorr   = rx .* complex(cos(2*pi*t_full/Fs*-f_offset), ...
                          sin(2*pi*t_full/Fs*-f_offset));
rxCFilt  = conv(rxCorr, h8000);
rxCFilt  = rxCFilt(1 : Fs/Fpass : end);

fprintf('f_offset = %.3f Hz applied. rxCFilt ready for Steps 18-21.\n', f_offset);

% =========================================================================
function update_slider_full(src, rx, t_full, Fs, h8000, Fpass, ax, sc, lbl)
% Slider callback: full CFO correction + conv() + decimate on each move.
    f_val        = get(src, 'Value');
    rxCorr       = rx .* complex(cos(2*pi*t_full/Fs*-f_val), ...
                                  sin(2*pi*t_full/Fs*-f_val));
    rxCFilt_tmp  = conv(rxCorr, h8000);
    rxCFilt_tmp  = rxCFilt_tmp(1 : Fs/Fpass : end);
    sc.XData     = real(rxCFilt_tmp);
    sc.YData     = imag(rxCFilt_tmp);
    r_max        = max(abs(rxCFilt_tmp)) * 1.15;
    axis(ax, [-r_max r_max -r_max r_max]);
    title(ax, sprintf('Step 15: f_{offset} = %.3f Hz', f_val), 'FontSize', 11);
    set(lbl, 'String', sprintf('%.2f Hz', f_val));
    drawnow;
end

%%======= HERE

f_offset = -630;
rxCorr   = rx .* complex(cos(2*pi*t_full/Fs*f_offset), sin(2*pi*t_full/Fs*f_offset));
rxCFilt  = conv(rxCorr, h8000);
rxCFilt  = rxCFilt(1 : Fs/Fpass : end);

%% STEP 18: QPSK Constellation Check (before preamble alignment)
figure('Name','Step 15 – Frequency-Corrected Constellation','NumberTitle','off');
scatter(real(rxCFilt), imag(rxCFilt), '.');
axis equal; grid on;
title('Step 15: Frequency-Corrected Constellation');
xlabel('I'); ylabel('Q');

%{
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
%}