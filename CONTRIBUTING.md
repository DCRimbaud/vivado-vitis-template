# Contributing

## Scope

Contributions must remain generic and reusable. Do not submit product register
maps, board pinouts, customer configuration, production bitstreams, proprietary
protocols, or generated Vitis workspaces.

## Required checks

1. Keep generated project placeholders explicit.
2. Add or update host-testable RTL testbenches for generic RTL changes.
3. Run `scripts\check_template_quality.ps1` and the relevant syntax checks.
4. Keep the Vivado/Vitis version assumptions explicit.
5. Document every new generated address, interrupt route, clock, and reset
   assumption in the platform guide.

## Comments

New public integration code and documentation must be written in English.
Firmware template snapshots may retain their original contract comments until a
deliberate template-normalization change is made; do not mix translations inside
one function contract.
