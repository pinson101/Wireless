%% CSE4377 Lab 6 - Demodulation and Reception
% =========================================================================
%  RTL-SDR data capture analysis 
% =========================================================================

clear; close all; clc;

IQ_FILE       = '/Users/angelina/Desktop/Wireless/AJJ_Lab06_Test4_QPSKpt2.iq';   % Path!!!

% Preamble bit sequence
PREAMBLE_BITS = [0 1 1 1 0 0 1 1 0 1 0 0 0 0 1 1]; % avoids 4th quadrant ASCII: "sC"

% for finding BER: what data bits are we transmitting, ASCII: "rtOH"
TX_DATA_BITS  = repmat([0 1 1 1 0 0 1 0 0 1 1 1 0 1 0 0 0 1 0 0 1 1 1 1 0 1 0 0 1 0 0 0], 1, 8);

%% STEP 12: Read IQ File
fOffset = -631; % frequency offset found with fft and animation code commented out
len  = 2048000/10; % only way we could use data
file = fopen(IQ_FILE, 'rb');
if file == -1
    error('Cannot open file "%s". Check IQ_FILE path.', IQ_FILE);
end
rx = zeros(len, 1);
for i = 1:len
    re    = (fread(file, 1, "uint8=>double") - 128);
    im    = (fread(file, 1, "uint8=>double") - 128);
    rx(i) = complex(re, im);
    rx(i) = rx(i) * complex(cos(2*pi*i/2048000*fOffset), sin(2*pi*i/2048000*fOffset)); %adjusts for freq off
end
fclose(file);
fprintf('  Read %d complex samples.\n', len);

% FIGURE 1: RAW on the IQ plane
figure('Name','Step 13 – Raw IQ Constellation with Frequency Offset Correction','NumberTitle','off');
%plot(real(rx), imag(rx), '.');
scatter(real(rx), imag(rx), '.');
axis equal; grid on;
title('Raw IQ Constellation with Frequency Offset Correction');
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
% plot each and see which gives tighter clusters
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
figure('Name','Step 15 – FFT','NumberTitle','off');
plot(fft_rxFilt);
title("Complex Magnitude of fft Spectrum")
xlabel("f (Hz)")
ylabel("|fft(X)|")

% find frequency index of the peak
[~, best_f_idx] = max(fft_rxFilt(1:floor(N/2)));
best_f = ((best_f_idx - 1) * Fs_dec / length(rxFilt))/4;
fprintf('  Estimated frequency offset: %.2f Hz\n', best_f);

%% STEP 19-20: DONE IN TWO DIFFERENT METHODS 
% 1. Preamble Definition & Correlation peak
% 2. Phase Rotation and Quadrant Decoding
%% ---------------------------------------------------------------------
%% METHOD 1: Define Expected Preamble and Find Through Correlation
%% ---------------------------------------------------------------------
n_pre_bits = length(PREAMBLE_BITS);
n_pre_syms = n_pre_bits / 2;
preamble_syms = zeros(n_pre_syms, 1);

% basically makes a wave copy of what the preamble should look like
% Bit pairs: 00->+1+j  01->-1+j  11->-1-j  10->+1-j
for k = 1:n_pre_syms
    b0 = PREAMBLE_BITS(2*k-1);
    b1 = PREAMBLE_BITS(2*k);
    if     b0==0 && b1==0;  preamble_syms(k) =  1+1j;   % +I +Q
    elseif b0==1 && b1==0;  preamble_syms(k) = -1+1j;   % -I +Q
    elseif b0==1 && b1==1;  preamble_syms(k) = -1-1j;   % -I -Q
    else;                   preamble_syms(k) =  1-1j;   % +I -Q  (b0=0,b1=1)
    end
end
preamble_syms = preamble_syms / norm(preamble_syms);   % normalize

% correlate received signal with preamble
corr = abs(conv(rxFilt, flipud(conj(preamble_syms)), 'same'));
N_rx    = length(rxFilt);
N_pre   = length(preamble_syms);    

[~, peak_idx] = max(corr);            % 1-based index into rxFilt
%peak_idx = 337;
fprintf('  Preamble peak at sample index %d \n', ...
        peak_idx);

figure('Name','Step 20 – Preamble Correlation','NumberTitle','off');
plot((0:length(corr)-1), corr, 'Color', [0.2 0.5 0.8], 'LineWidth', 0.8);
hold on;
plot(peak_idx-1, corr(peak_idx), 'r*', 'MarkerSize', 12, 'LineWidth', 2);
hold off;
grid on;
title('Step 20: Preamble Correlation Output');
xlabel('Sample Index (at 8 ksps)'); ylabel('|Correlation|');
legend('Correlation Magnitude', sprintf('Peak at index %d', peak_idx-1), ...
       'Location','best');

%% STEP 21: Phase Rotation Correction, Demodulation, and BER
data_start = peak_idx + N_pre;   % data starts at preamble start + preamble length

% find phase rotation based on real constelation vs expected 
rx_pre_window = rxFilt(peak_idx : peak_idx + N_pre - 1);
phase_est     = angle(sum(rx_pre_window .* conj(preamble_syms)));
phase_est = 6.8;
fprintf('  Estimated constellation rotation: %.2f deg\n', rad2deg(phase_est));

%Preamble Comparison (IQ Domain)
figure('Name','Preamble Comparison: Received vs. Ideal','NumberTitle','off');

% Plot the received preamble window (after phase correction)
rx_pre_corrected = rx_pre_window * exp(-1j * phase_est);
s1 = scatter(real(rx_pre_corrected), imag(rx_pre_corrected), 60, 'filled', 'MarkerFaceAlpha', 0.6);
hold on;

% Plot the ideal preamble symbols (normalized/scaled to match)
ideal_scaled = preamble_syms * mean(abs(rx_pre_window));
s2 = scatter(real(ideal_scaled), imag(ideal_scaled), 100, 'r', 'LineWidth', 2);

% Formatting
grid on; axis equal;
xline(0, 'k--'); yline(0, 'k--');
xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');
title('Preamble Alignment: Ideal vs. Received (Phase Corrected)');
legend([s1, s2], {'Received Samples (rx\_pre\_window)', 'Ideal Preamble (preamble\_syms)'}, ...
       'Location', 'northeastoutside');

% Display the first few symbols for manual verification
fprintf('\n--- Preamble Symbol Check ---\n');
for i = 1:min(4, length(preamble_syms))
    fprintf('Symbol %d: Expected (%.2f + %.2fj) | Received (%.2f + %.2fj)\n', ...
        i, real(ideal_scaled(i)), imag(ideal_scaled(i)), ...
        real(rx_pre_corrected(i)), imag(rx_pre_corrected(i)));
end

%% ---------------------------------------------------------------------
%% METHOD 2: Rotate, Demodulate, Find Preamble
%% ---------------------------------------------------------------------
% plot the RAW rx data rotated by phase_est (make sure matches expected)
rx_raw_rotated = rx * exp(-1j * phase_est);

figure('Name','Step 21a – Raw RX Rotated','NumberTitle','off');
scatter(real(rx_raw_rotated), imag(rx_raw_rotated), '.');
hold on;
xline(0, 'k--', 'LineWidth', 1.2);
yline(0, 'k--', 'LineWidth', 1.2);
hold off;
axis equal; grid on;
title(sprintf('Step 21a: Raw RX Rotated by phase\\_est = %.3f rad (%.1f°)', ...
              phase_est, rad2deg(phase_est)));
xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');

% plot the FILTERED data rotated with decision boundaries
rxFilt_rotated = rxFilt * exp(-1j * phase_est);

figure('Name','Step 21b – Filtered RX Rotated with Decision Boundaries','NumberTitle','off');
scatter(real(rxFilt_rotated), imag(rxFilt_rotated), '.');
hold on;
xline(0, 'k--', 'LineWidth', 1.8);
yline(0, 'k--', 'LineWidth', 1.8);
ax_lim = max(abs([real(rxFilt_rotated); imag(rxFilt_rotated)])) * 0.75;
hold off;
axis equal; grid on;
title(sprintf('Step 21b: Filtered RX Rotated (%.1f°) with QPSK Decision Boundaries', ...
              rad2deg(phase_est)));
xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');

% samples into a flat bit stream
n_syms_total = length(rxFilt_rotated);
all_bits = zeros(n_syms_total * 2, 1);

for k = 1:n_syms_total
    s = rxFilt_rotated(k);
    I = real(s);  Q = imag(s);
    if     I >= 0 && Q >= 0;  bits = [0; 0];   % +I+Q → b0=0, b1=0
    elseif I <  0 && Q >= 0;  bits = [1; 0];   % -I+Q → b0=1, b1=0
    elseif I <  0 && Q <  0;  bits = [1; 1];   % -I-Q → b0=1, b1=1
    else;                      bits = [0; 1];   % +I-Q → b0=0, b1=1
    end
end

fprintf('\n--- Full bit stream decoded: %d bits total ---\n', length(all_bits));

%%  Search for the preamble pattern in the full bit stream
%     Slide the preamble window across all_bits and count matches at each position.

pre_row   = PREAMBLE_BITS(:);          % column vector of preamble bits
n_pre_b   = length(pre_row);
n_bits    = length(all_bits);
match_scores = zeros(n_bits - n_pre_b, 1);

for pos = 1 : (n_bits - n_pre_b)
    window = all_bits(pos : pos + n_pre_b - 1);
    match_scores(pos) = sum(window == pre_row);   % count matching bits
end

% A perfect match scores n_pre_b. Accept anything within 1 bit error.
MATCH_THRESHOLD = n_pre_b - 1;   % allow 1 bit error in preamble detection
preamble_positions = find(match_scores >= MATCH_THRESHOLD);

%% 5. Announce preamble detection results
if isempty(preamble_positions)
    fprintf('\n*** PREAMBLE NOT FOUND in bit stream (threshold = %d/%d bits) ***\n', ...
            MATCH_THRESHOLD, n_pre_b);
    fprintf('    Try adjusting phase_est or check preamble bit mapping.\n');
else
    fprintf('\n*** PREAMBLE FOUND ***\n');
    fprintf('    Detected at %d bit position(s): ', length(preamble_positions));
    fprintf('%d ', preamble_positions(1:min(10,end)));
    if length(preamble_positions) > 10; fprintf('...'); end
    fprintf('\n');
    fprintf('    First preamble at bit index %d (symbol %d)\n', ...
            preamble_positions(1), ceil(preamble_positions(1)/2));

    % Plot match score so all preamble locations are visible
    figure('Name','Step 21c – Preamble Search in Bit Stream','NumberTitle','off');
    plot(match_scores, 'Color',[0.2 0.5 0.8], 'LineWidth', 0.7);
    hold on;
    yline(MATCH_THRESHOLD, 'k--', sprintf('Threshold (%d/%d)', MATCH_THRESHOLD, n_pre_b), ...
          'LineWidth', 1.2, 'LabelHorizontalAlignment','left');
    plot(preamble_positions, match_scores(preamble_positions), ...
         'r*', 'MarkerSize', 8, 'LineWidth', 1.5);
    hold off; grid on;
    title(sprintf('Step 21c: Preamble Search — %d match(es) found', ...
                  length(preamble_positions)));
    xlabel('Bit Index'); ylabel(sprintf('Matching bits out of %d', n_pre_b));
    legend('Match score','Threshold','Preamble hit','Location','best');
end

%% BER on the 256 bits immediately after the first detected preamble
if ~isempty(preamble_positions)

    first_pre_bit  = preamble_positions(1);
    data_bit_start = first_pre_bit + n_pre_b;      % first data bit index
    data_bit_end   = data_bit_start + 256 - 1;     % 256 bits = 128 QPSK symbols

    if data_bit_end > n_bits
        fprintf('\n  Not enough bits after preamble for 256-bit BER window (only %d available).\n', ...
                n_bits - data_bit_start + 1);
        data_bit_end = n_bits;
    end

    rx_data_bits = all_bits(data_bit_start : data_bit_end);
    n_data_bits  = length(rx_data_bits);

    fprintf('\n--- BER on %d bits after first preamble (bit %d to %d) ---\n', ...
            n_data_bits, data_bit_start, data_bit_end);
    fprintf('  RX bits: ');
    fprintf('%d', rx_data_bits(1:min(64,end)));
    if n_data_bits > 64; fprintf(' ...'); end
    fprintf('\n');

    if ~isempty(TX_DATA_BITS)
        tx_col    = TX_DATA_BITS(:);
        n_compare = min(length(tx_col), n_data_bits);
        tx_window = tx_col(1:n_compare);
        rx_window = rx_data_bits(1:n_compare);

        n_err = sum(rx_window ~= tx_window);
        BER   = n_err / n_compare;

        fprintf('  TX bits: ');
        fprintf('%d', tx_window(1:min(64,end)));
        if n_compare > 64; fprintf(' ...'); end
        fprintf('\n');
        fprintf('\n  BER = %d errors / %d bits = %.6f\n', n_err, n_compare, BER);

        ber_str = sprintf('BER = %.4f  (%d errors / %d bits)', BER, n_err, n_compare);

        % Bit error location plot
        figure('Name','Step 21d – Bit Error Locations','NumberTitle','off');
        bit_errors = rx_window ~= tx_window;
        stem(find(bit_errors), ones(sum(bit_errors),1), 'r', ...
             'MarkerSize', 4, 'LineWidth', 0.8);
        grid on;
        title({sprintf('Step 21d: Bit Error Locations  —  %s', ber_str), ...
               sprintf('First preamble at bit %d, data window: bits %d–%d', ...
                       first_pre_bit, data_bit_start, data_bit_end)});
        xlabel('Bit Index (within 256-bit window)'); ylabel('Error (1 = wrong)');
        ylim([0 1.4]);

    else
        fprintf('  TX_DATA_BITS not set — skipping BER calculation.\n');
        ber_str = 'BER: TX_DATA_BITS not set';
    end

    % Final constellation showing only the 256 data bits window
    data_sym_start = data_bit_start / 2;
    data_sym_end   = ceil(data_bit_end / 2);
    data_syms_plot = rxFilt_rotated(data_sym_start : min(data_sym_end, length(rxFilt_rotated)));

    figure('Name','Step 21e – Data Block Constellation (256 bits)','NumberTitle','off');
    scatter(real(data_syms_plot), imag(data_syms_plot), 20, [0.15 0.55 0.9], 'filled', ...
            'MarkerFaceAlpha', 0.6);
    hold on;
    xline(0, 'k--', 'LineWidth', 1.5);
    yline(0, 'k--', 'LineWidth', 1.5);
    amp_d  = mean(abs(data_syms_plot));
    ideal_d = amp_d * [1+1j, -1+1j, -1-1j, 1-1j];
    plot(real(ideal_d), imag(ideal_d), 'r+', 'MarkerSize', 18, 'LineWidth', 2.5);
    text( amp_d*0.6,  amp_d*0.6, '00','FontSize',11,'FontWeight','bold','Color',[0.1 0.5 0.1]);
    text(-amp_d*0.6,  amp_d*0.6, '01','FontSize',11,'FontWeight','bold','Color',[0.1 0.5 0.1]);
    text(-amp_d*0.6, -amp_d*0.6, '11','FontSize',11,'FontWeight','bold','Color',[0.1 0.5 0.1]);
    text( amp_d*0.6, -amp_d*0.6, '10','FontSize',11,'FontWeight','bold','Color',[0.1 0.5 0.1]);
    hold off;
    axis equal; grid on;
    title({'Step 21e: 256-bit Data Block Constellation', ber_str});
    xlabel('In-Phase (I)'); ylabel('Quadrature (Q)');
    legend('Data Symbols','Decision Boundaries','Ideal QPSK','Location','best');
end






% get data
n_data = N_rx - data_start;   % remaining symbols after preamble
rx_data_raw  = rxFilt(data_start : data_start + n_data - 1);
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

% BER Calculation
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


%{
offset_estimate = 630; %from fft

% -------------------------------------------------------------------------
% 2.  Define sweep range
% -------------------------------------------------------------------------
sweep_half  = 10;     % ± Hz around estimate
sweep_step  = 0.1;    % Hz per frame
frame_pause = 0.12;   % seconds between frames (filtering is the bottleneck anyway)

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

sc = scatter(ax, real(rxCFilt_init), imag(rxCFilt_init), '.');

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

%}