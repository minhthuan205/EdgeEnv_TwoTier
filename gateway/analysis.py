"""Transparent offload baseline, NOT a trained model or validated forecast."""
import math


def analyze(samples):
    if not isinstance(samples, list) or not 3 <= len(samples) <= 120:
        raise ValueError("Need 3..120 [relative_ms, temperature_c, humidity_rh] samples")
    rows = []
    for row in samples:
        if not isinstance(row, list) or len(row) != 3:
            raise ValueError("Bad row shape")
        if any(isinstance(x, bool) or not isinstance(x, (float, int)) or not math.isfinite(x) for x in row):
            raise ValueError("Nonfinite/non-numeric value")
        ms, temp, hum = row
        if not 0 <= ms <= 1200000 or not -40 <= temp <= 125 or not 0 <= hum <= 100:
            raise ValueError("Out of sensor range")
        if rows and ms <= rows[-1][0]:
            raise ValueError("Timestamps must increase")
        rows.append((ms, temp, hum))
    xs = [(r[0]-rows[0][0])/1000 for r in rows]
    ys = [r[1] for r in rows]
    xm, ym = sum(xs)/len(xs), sum(ys)/len(ys)
    denom = sum((x-xm)**2 for x in xs)
    slope = sum((x-xm)*(y-ym) for x, y in zip(xs, ys))/denom
    # Engineering trial threshold, not calibrated to deployment data.
    label = "rapid_rise_candidate" if slope >= 0.5 else "no_rapid_rise_by_baseline"
    residual = sum((y-(ym+slope*(x-xm)))**2 for x, y in zip(xs, ys))/len(xs)
    return {
        "method": "linear_trend_baseline", "trained_ai": False,
        "label": label, "slope_c_per_s": slope,
        "fit_rmse_c": math.sqrt(residual), "window_seconds": xs[-1],
        "forecast": None,
        "note": "Candidate trend only; cannot establish cause or guarantee future temperature."
    }
