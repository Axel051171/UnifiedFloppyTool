#!/usr/bin/env python3
"""
FloppyAI Metrics Module
Provides metrics calculations and plotting for experiment analysis.
"""
import json
import numpy as np
import matplotlib.pyplot as plt
from pathlib import Path
from typing import Dict, List, Any, Optional, Tuple
import datetime


class ExperimentMetrics:
    """Calculates and analyzes experiment metrics."""

    def __init__(self, experiment_results: Dict[str, Any]):
        self.results = experiment_results
        self.metrics = self._calculate_metrics()

    def _calculate_metrics(self) -> Dict[str, Any]:
        """Calculate comprehensive metrics from experiment results."""
        metrics = {
            'summary': {},
            'by_pattern': {},
            'by_density': {},
            'by_track': {},
            'correlation_analysis': {},
            'jitter_analysis': {},
            'spectral_analysis': {}
        }

        if not self.results.get('matrix_runs'):
            return metrics

        runs = self.results['matrix_runs']
        successful_runs = [r for r in runs if r.get('success', False)]

        if not successful_runs:
            return metrics

        # Overall summary metrics
        metrics['summary'] = {
            'total_runs': len(runs),
            'successful_runs': len(successful_runs),
            'success_rate': len(successful_runs) / len(runs) if runs else 0,
            'unique_patterns': len(set(r['parameters']['pattern'] for r in successful_runs)),
            'unique_densities': len(set(r['parameters']['density'] for r in successful_runs)),
            'unique_tracks': len(set(r['parameters']['track'] for r in successful_runs))
        }

        # Metrics by pattern
        patterns = {}
        for run in successful_runs:
            pattern = run['parameters']['pattern']
            if pattern not in patterns:
                patterns[pattern] = []

            # Extract key metrics from run
            run_metrics = run.get('metrics', {})
            patterns[pattern].append({
                'density': run_metrics.get('density_estimate', 0),
                'mean_interval': run_metrics.get('mean_interval_ns', 0),
                'total_fluxes': run_metrics.get('total_fluxes', 0),
                'track': run['parameters']['track'],
                'side': run['parameters']['side']
            })

        metrics['by_pattern'] = {}
        for pattern, data in patterns.items():
            if data:
                densities = [d['density'] for d in data]
                intervals = [d['mean_interval'] for d in data]
                fluxes = [d['total_fluxes'] for d in data]

                metrics['by_pattern'][pattern] = {
                    'count': len(data),
                    'avg_density': np.mean(densities) if densities else 0,
                    'std_density': np.std(densities) if densities else 0,
                    'avg_interval': np.mean(intervals) if intervals else 0,
                    'std_interval': np.std(intervals) if intervals else 0,
                    'avg_fluxes': np.mean(fluxes) if fluxes else 0,
                    'min_density': np.min(densities) if densities else 0,
                    'max_density': np.max(densities) if densities else 0
                }

        # Metrics by density
        densities = {}
        for run in successful_runs:
            density = run['parameters']['density']
            if density not in densities:
                densities[density] = []

            run_metrics = run.get('metrics', {})
            densities[density].append({
                'pattern': run['parameters']['pattern'],
                'density_estimate': run_metrics.get('density_estimate', 0),
                'mean_interval': run_metrics.get('mean_interval_ns', 0),
                'total_fluxes': run_metrics.get('total_fluxes', 0)
            })

        metrics['by_density'] = {}
        for density, data in densities.items():
            if data:
                estimates = [d['density_estimate'] for d in data]
                metrics['by_density'][density] = {
                    'count': len(data),
                    'avg_estimate': np.mean(estimates) if estimates else 0,
                    'std_estimate': np.std(estimates) if estimates else 0,
                    'accuracy': np.mean(estimates) / density if density > 0 else 0,
                    'precision': 1 - np.std(estimates) / np.mean(estimates) if estimates and np.mean(estimates) > 0 else 0
                }

        return metrics

    def get_jitter_metrics(self) -> Dict[str, Any]:
        """Calculate jitter-related metrics."""
        if not self.results.get('matrix_runs'):
            return {}

        runs = [r for r in self.results['matrix_runs'] if r.get('success', False)]
        if not runs:
            return {}

        jitter_data = []

        for run in runs:
            metrics = run.get('metrics', {})
            mean_interval = metrics.get('mean_interval_ns', 0)
            if mean_interval > 0:
                # Estimate jitter as coefficient of variation of intervals
                # In a real implementation, this would use actual interval data
                jitter_data.append({
                    'pattern': run['parameters']['pattern'],
                    'density': run['parameters']['density'],
                    'jitter_estimate': metrics.get('std_interval', 0) / mean_interval if mean_interval > 0 else 0
                })

        if jitter_data:
            patterns = {}
            for data in jitter_data:
                pattern = data['pattern']
                if pattern not in patterns:
                    patterns[pattern] = []
                patterns[pattern].append(data['jitter_estimate'])

            return {
                'by_pattern': {pattern: {
                    'avg_jitter': np.mean(values),
                    'std_jitter': np.std(values),
                    'min_jitter': np.min(values),
                    'max_jitter': np.max(values)
                } for pattern, values in patterns.items()},
                'overall_avg_jitter': np.mean([d['jitter_estimate'] for d in jitter_data])
            }

        return {}

    def get_correlation_metrics(self) -> Dict[str, Any]:
        """Calculate correlation between intended and measured parameters."""
        if not self.results.get('matrix_runs'):
            return {}

        runs = [r for r in self.results['matrix_runs'] if r.get('success', False)]
        if not runs:
            return {}

        correlation_data = []

        for run in runs:
            intended_density = run['parameters']['density']
            measured_density = run['metrics'].get('density_estimate', 0)

            correlation_data.append({
                'intended': intended_density,
                'measured': measured_density,
                'error': measured_density - intended_density,
                'relative_error': (measured_density - intended_density) / intended_density if intended_density > 0 else 0
            })

        if correlation_data:
            intended = [d['intended'] for d in correlation_data]
            measured = [d['measured'] for d in correlation_data]
            errors = [d['error'] for d in correlation_data]

            correlation = np.corrcoef(intended, measured)[0, 1] if len(intended) > 1 else 0

            return {
                'correlation_coefficient': correlation,
                'mean_absolute_error': np.mean(np.abs(errors)),
                'root_mean_square_error': np.sqrt(np.mean(np.array(errors)**2)),
                'mean_relative_error': np.mean([d['relative_error'] for d in correlation_data])
            }

        return {}

    def plot_results(self, output_dir: Path) -> List[str]:
        """Generate plots for experiment results."""
        plot_files = []

        try:
            # Plot 1: Density by Pattern
            self._plot_density_by_pattern(output_dir / 'density_by_pattern.png')
            plot_files.append(str(output_dir / 'density_by_pattern.png'))

            # Plot 2: Density vs Intended Density
            self._plot_density_correlation(output_dir / 'density_correlation.png')
            plot_files.append(str(output_dir / 'density_correlation.png'))

            # Plot 3: Success Rate by Pattern
            self._plot_success_rate(output_dir / 'success_rate_by_pattern.png')
            plot_files.append(str(output_dir / 'success_rate_by_pattern.png'))

            # Plot 4: Jitter Analysis
            jitter_metrics = self.get_jitter_metrics()
            if jitter_metrics:
                self._plot_jitter_analysis(jitter_metrics, output_dir / 'jitter_analysis.png')
                plot_files.append(str(output_dir / 'jitter_analysis.png'))

        except Exception as e:
            print(f"Warning: Plotting failed: {e}")

        return plot_files

    def _plot_density_by_pattern(self, output_path: Path):
        """Plot average density by pattern."""
        by_pattern = self.metrics.get('by_pattern', {})
        if not by_pattern:
            return

        patterns = list(by_pattern.keys())
        avg_densities = [by_pattern[p]['avg_density'] for p in patterns]
        std_densities = [by_pattern[p]['std_density'] for p in patterns]

        fig, ax = plt.subplots(figsize=(10, 6))
        ax.bar(patterns, avg_densities, yerr=std_densities, capsize=5)
        ax.set_title('Average Density by Pattern')
        ax.set_xlabel('Pattern')
        ax.set_ylabel('Density (bits/rev)')
        ax.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

    def _plot_density_correlation(self, output_path: Path):
        """Plot correlation between intended and measured density."""
        runs = [r for r in self.results.get('matrix_runs', []) if r.get('success', False)]
        if not runs:
            return

        intended = []
        measured = []

        for run in runs:
            intended.append(run['parameters']['density'])
            measured.append(run['metrics'].get('density_estimate', 0))

        if intended and measured:
            fig, ax = plt.subplots(figsize=(8, 6))
            ax.scatter(intended, measured, alpha=0.6)
            ax.set_title('Intended vs Measured Density')
            ax.set_xlabel('Intended Density')
            ax.set_ylabel('Measured Density (bits/rev)')

            # Add trend line
            if len(intended) > 1:
                z = np.polyfit(intended, measured, 1)
                p = np.poly1d(z)
                ax.plot(intended, p(intended), "r--", alpha=0.8)

            ax.grid(True, alpha=0.3)
            plt.tight_layout()
            plt.savefig(output_path, dpi=150, bbox_inches='tight')
            plt.close()

    def _plot_success_rate(self, output_path: Path):
        """Plot success rate by pattern."""
        by_pattern = self.metrics.get('by_pattern', {})
        if not by_pattern:
            return

        patterns = list(by_pattern.keys())
        counts = [by_pattern[p]['count'] for p in patterns]

        # Calculate success rates (simplified - all runs in by_pattern are successful)
        success_rates = [1.0] * len(patterns)  # All runs in by_pattern are successful

        fig, ax = plt.subplots(figsize=(10, 6))
        bars = ax.bar(patterns, success_rates, alpha=0.7)

        # Add count labels
        for bar, count in zip(bars, counts):
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height + 0.01,
                   f'n={count}', ha='center', va='bottom')

        ax.set_title('Success Rate by Pattern')
        ax.set_xlabel('Pattern')
        ax.set_ylabel('Success Rate')
        ax.set_ylim(0, 1.1)
        ax.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

    def _plot_jitter_analysis(self, jitter_metrics: Dict[str, Any], output_path: Path):
        """Plot jitter analysis results."""
        by_pattern = jitter_metrics.get('by_pattern', {})
        if not by_pattern:
            return

        patterns = list(by_pattern.keys())
        avg_jitter = [by_pattern[p]['avg_jitter'] for p in patterns]
        std_jitter = [by_pattern[p]['std_jitter'] for p in patterns]

        fig, ax = plt.subplots(figsize=(10, 6))
        ax.bar(patterns, avg_jitter, yerr=std_jitter, capsize=5)
        ax.set_title('Average Jitter by Pattern')
        ax.set_xlabel('Pattern')
        ax.set_ylabel('Jitter Estimate')
        ax.grid(True, alpha=0.3)
        plt.tight_layout()
        plt.savefig(output_path, dpi=150, bbox_inches='tight')
        plt.close()

    def generate_report(self, output_dir: Path) -> str:
        """Generate a comprehensive experiment report."""
        report_lines = [
            "# FloppyAI Experiment Report",
            f"Generated: {datetime.datetime.now().isoformat()}",
            "",
            "## Summary",
        ]

        summary = self.metrics.get('summary', {})
        if summary:
            report_lines.extend([
                f"- Total runs: {summary['total_runs']}",
                f"- Successful runs: {summary['successful_runs']}",
                f"- Success rate: {summary['success_rate']".2%"}",
                f"- Unique patterns tested: {summary['unique_patterns']}",
                f"- Unique densities tested: {summary['unique_densities']}",
                f"- Unique tracks tested: {summary['unique_tracks']}",
                ""
            ])

        # Pattern performance
        report_lines.append("## Pattern Performance")
        by_pattern = self.metrics.get('by_pattern', {})
        if by_pattern:
            for pattern, metrics in by_pattern.items():
                report_lines.append(f"### {pattern}")
                report_lines.append(f"- Average density: {metrics['avg_density']".1f"} ± {metrics['std_density']:".1f" bits/rev")
                report_lines.append(f"- Count: {metrics['count']} runs")
                report_lines.append("")

        # Density accuracy
        report_lines.append("## Density Accuracy")
        by_density = self.metrics.get('by_density', {})
        if by_density:
            for density, metrics in by_density.items():
                report_lines.append(f"### Density {density}")
                report_lines.append(f"- Average measured: {metrics['avg_estimate']".2f"}")
                report_lines.append(f"- Accuracy: {metrics['accuracy']".2%"}")
                report_lines.append(f"- Precision: {metrics['precision']".2%"}")
                report_lines.append("")

        # Jitter analysis
        jitter = self.get_jitter_metrics()
        if jitter:
            report_lines.append("## Jitter Analysis")
            report_lines.append(f"- Overall average jitter: {jitter['overall_avg_jitter']".4f"}")
            for pattern, metrics in jitter.get('by_pattern', {}).items():
                report_lines.append(f"- {pattern}: {metrics['avg_jitter']".4f"} ± {metrics['std_jitter']".4f"")
            report_lines.append("")

        # Correlation analysis
        correlation = self.get_correlation_metrics()
        if correlation:
            report_lines.append("## Correlation Analysis")
            report_lines.extend([
                f"- Correlation coefficient: {correlation['correlation_coefficient']".3f"}",
                f"- Mean absolute error: {correlation['mean_absolute_error']".2f"}",
                f"- Root mean square error: {correlation['root_mean_square_error']".2f"}",
                f"- Mean relative error: {correlation['mean_relative_error']".2%"}",
                ""
            ])

        # Save report
        report_path = output_dir / 'experiment_report.md'
        with open(report_path, 'w') as f:
            f.write('\n'.join(report_lines))

        return str(report_path)


def analyze_experiment_results(results_file: Path) -> ExperimentMetrics:
    """Load and analyze experiment results."""
    try:
        with open(results_file, 'r') as f:
            results = json.load(f)
        return ExperimentMetrics(results)
    except Exception as e:
        print(f"Error loading experiment results: {e}")
        return ExperimentMetrics({})


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python -m FloppyAI.src.analysis.metrics <results_file>")
        sys.exit(1)

    results_file = Path(sys.argv[1])
    metrics = analyze_experiment_results(results_file)

    # Generate plots and report
    output_dir = results_file.parent
    plot_files = metrics.plot_results(output_dir)
    report_file = metrics.generate_report(output_dir)

    print(f"Analysis complete!")
    print(f"Plots saved: {plot_files}")
    print(f"Report saved: {report_file}")
