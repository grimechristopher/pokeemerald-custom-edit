"""Sanity-checks every authored route in data.py. Run after adding routes."""
import sys

from data import ROUTES
from tiers import expand_tier_table


def main():
    for map_id, route in ROUTES.items():
        for key in ("is_johto", "base_label_prefix", "day", "night"):
            if key not in route:
                print(f"FAIL: {map_id} missing '{key}'")
                sys.exit(1)
        try:
            expand_tier_table(route["day"], route["is_johto"])
            expand_tier_table(route["night"], route["is_johto"])
        except ValueError as e:
            print(f"FAIL: {map_id}: {e}")
            sys.exit(1)
    print(f"OK: {len(ROUTES)} routes validated")


if __name__ == "__main__":
    main()
