/*
 *    Copyright (C) 2016-2025 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

class FlowComponent
{
public:
  FlowComponent* addTo(tf::Taskflow& composition)
  {
    compositionTask_ = composition.composed_of(componentFlow_);
    return this;
  }
  FlowComponent* precede(FlowComponent& successor)
  {
    return precede(&successor);
  }
  FlowComponent* precede(FlowComponent* successor)
  {
    assert(successor);
    compositionTask_.precede(successor->compositionTask_);
    return this;
  }
  FlowComponent* name(const std::string& name)
  {
    compositionTask_.name(name);
    return this;
  }
  tf::Task& nextTask()
  {
    componentTasks_.push(componentFlow_.placeholder());
    return componentTasks_.back();
  }

private:
  std::queue<tf::Task> componentTasks_;
  tf::Taskflow componentFlow_;
  tf::Task compositionTask_;
};
