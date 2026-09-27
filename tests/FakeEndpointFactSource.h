#pragma once

#include "gigante/EndpointPolicy.h"

#include <utility>
#include <vector>

class FakeEndpointFactSource final : public gigante::EndpointFactSource
{
public:
    void replace(std::vector<gigante::AudioEndpoint> endpoints)
    {
        ++inventory.generation;
        inventory.endpoints = std::move(endpoints);
    }

    [[nodiscard]] gigante::EndpointInventory snapshot() const override
    {
        return inventory;
    }

private:
    gigante::EndpointInventory inventory;
};
