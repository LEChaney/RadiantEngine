See [Monte Carlo Integration](https://www.scratchapixel.com/lessons/mathematics-physics-for-computer-graphics/monte-carlo-methods-in-practice/monte-carlo-integration.html) for a more detailed explanation of Monte Carlo integration using the Monte Carlo estimator.

**Definition (integral over domain $D$):**
Let $I = \int_D f(x)\,dx$. Draw $N$ independent samples $X_i$ from a probability density $p(x) > 0$ wherever $f(x) \ne 0$. The Monte Carlo (importance sampling) estimator is
\[
\widehat{I}_N = \frac{1}{N}\sum_{i=1}^N \frac{f(X_i)}{p(X_i)}.
\]

**Integral vs estimator (side by side)**:
\[
\boxed{I = \int_D f(x)\,dx}\;\Longleftrightarrow\;\boxed{\widehat{I}_N = \frac{1}{N}\sum_{i=1}^N \frac{f(X_i)}{p(X_i)}}
\]

**Derivation from expectation:**
Expectation definition: for a PDF $p(x)$ on $D$ and any function $g$,
\[
\mathbb{E}_p[g(X)] = \int_D g(x)\, p(x)\,dx.
\]
Steps:
1. Want $I = \int_D f(x)\,dx$.
2. Let $g(x) = f(x)/p(x)$ where $p(x) > 0$ whenever $f(x) \ne 0$.
3. Plug $g$ into the expectation definition:
	\[
	\mathbb{E}_p\!\left[\frac{f(X)}{p(X)}\right] = \int_D \frac{f(x)}{p(x)} p(x)\,dx = \int_D f(x)\,dx = I.
	\]
4. Replace the expectation by the sample mean of i.i.d. draws $X_1,\dots,X_N \sim p$:
	\[
	\widehat{I}_N = \frac{1}{N}\sum_{i=1}^N \frac{f(X_i)}{p(X_i)}.
	\]
5. Unbiasedness: $\mathbb{E}[\widehat{I}_N] = I$ because expectation of a sample mean equals the true expectation.

**Uniform sampling special case:**
If $p$ is uniform on a finite-measure domain $D$ ($p(x)=1/|D|$) then
\[
\widehat{I}_N = |D|\, \frac{1}{N}\sum_{i=1}^N f(X_i) = |D|\, \overline{f}_N,
\]
where $\overline{f}_N$ is the average of the sampled function values.

> **Why importance sampling helps:**
> Variance depends on how much $f(x)/p(x)$ fluctuates. Choosing $p(x) \propto |f(x)|$ makes $f(x)/p(x)$ nearly constant, minimizing variance in the ideal case.

**Unbiasedness:**
\[
\mathbb{E}[\widehat{I}_N] = I.
\]
Variance:
\[
\operatorname{Var}[\widehat{I}_N] = \frac{1}{N}\left(\int_D \frac{f(x)^2}{p(x)}\,dx - I^2\right) = O\!\left(\frac{1}{N}\right).
\]
> Note: O is Big-O notation: here $\operatorname{Var}[\widehat{I}_N] = O(1/N)$ means there exists a constant $C$ (independent of $N$ and the samples) such that $\operatorname{Var}[\widehat{I}_N] \le C/N$ for all sufficiently large $N$. It says the variance shrinks proportionally to $1/N$ (up to a constant factor). Consequently the standard deviation (error bar) shrinks like $O(1/\sqrt{N})$.

**Convergence (CLT):**
\[
\sqrt{N}\big(\widehat{I}_N - I\big) \xrightarrow{d} \mathcal{N}(0,\sigma^2),\quad
\sigma^2 = \int_D \frac{f(x)^2}{p(x)}\,dx - I^2.
\]
> Note: As you take more samples, the distribution of the scaled error \(\sqrt{N}(\widehat{I}_N - I)\) becomes bell-shaped (normal) with mean 0 and variance \(\sigma^2\). So the raw error \(\widehat{I}_N - I\) typically has size about \(\sigma/\sqrt{N}\). This lets you form approximate confidence intervals: \(\widehat{I}_N \pm z_{\alpha/2}\, (\sigma/\sqrt{N})\). In practice you plug in the sample standard deviation \(s\) for \(\sigma\): e.g. a 95% interval \(\widehat{I}_N \pm 1.96\, s/\sqrt{N}\) (valid for large \(N\)).
> Practical standard error estimate:
\[
s^2 = \frac{1}{N-1}\sum_{i=1}^N \left(\frac{f(X_i)}{p(X_i)} - \widehat{I}_N\right)^2,\qquad
	\text{SE} \approx \frac{s}{\sqrt{N}}.
\]

**Special case (uniform sampling):**
If $D$ has finite measure $|D|$ and samples are uniform, $p(x)=1/|D|$, then
\[
\widehat{I}_N = |D|\, \frac{1}{N}\sum_{i=1}^N f(X_i).
\]

**Expectation form:**
For an expectation $\mu = \mathbb{E}_p[g(X)] = \int g(x) p(x)\,dx$,
\[
\widehat{\mu}_N = \frac{1}{N}\sum_{i=1}^N g(X_i).
\]

**Key points:**
- Unbiased
- Root-mean-square error decays as $1/\sqrt{N}$
- Variance minimized by choosing $p(x) \propto |f(x)|$
- Independent of dimensionality (rate unchanged), making it suitable for high-dimensional integrals
- Stratification, importance sampling, and control variates reduce variance without bias