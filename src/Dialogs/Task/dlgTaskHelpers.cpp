// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "dlgTaskHelpers.hpp"
#include "Dialogs/TextEntry.hpp"
#include "Language/Language.hpp"
#include "Units/Units.hpp"
#include "Task/TypeStrings.hpp"
#include "Task/ValidationErrorStrings.hpp"
#include "Task/SaveFile.hpp"
#include "Task/ObservationZones/CylinderZone.hpp"
#include "Task/ObservationZones/SectorZone.hpp"
#include "Task/ObservationZones/LineSectorZone.hpp"
#include "Task/ObservationZones/KeyholeZone.hpp"
#include "Task/Shapes/FAITriangleTask.hpp"
#include "Engine/Task/Ordered/OrderedTask.hpp"
#include "Engine/Task/Points/Type.hpp"
#include "Engine/Task/Factory/AbstractTaskFactory.hpp"
#include "LocalPath.hpp"
#include "system/Path.hpp"

#include <cassert>

/**
 *
 * @param task
 * @param text
 * @return True if FAI shape
 */
static bool
TaskSummaryShape(const OrderedTask *task, TCHAR *text)
{
  bool FAIShape = false;
  switch (task->TaskSize()) {
  case 0:
    text[0] = '\0';
    break;

  case 1:
    _tcscpy(text, _("Unknown"));
    break;

  case 2:
    _tcscpy(text, _("Goal"));
    FAIShape = true;

    break;

  case 3:
    if (task->GetFactory().IsClosed()) {
      _tcscpy(text, _("Out and return"));
      FAIShape = true;
    }
    else
      _tcscpy(text, _("Two legs"));
    break;

  case 4:
    if (!task->GetFactory().IsUnique() ||!task->GetFactory().IsClosed())
      _tcscpy(text, _("Three legs"));
    else if (FAITriangleValidator::Validate(*task)) {
      _tcscpy(text, _("FAI triangle"));
      FAIShape = true;
    }
    else
      _tcscpy(text, _("non-FAI triangle"));
    break;

  default:
    StringFormatUnsafe(text, _("%d legs"), task->TaskSize() - 1);
    break;
  }
  return FAIShape;
}
void
OrderedTaskSummary(const OrderedTask *task, TCHAR *text, bool linebreaks)
{
  const TaskStats &stats = task->GetStats();
  TCHAR summary_shape[100];
  bool FAIShape = TaskSummaryShape(task, summary_shape);
  TaskValidationErrorSet validation_errors;
  if (FAIShape || task->GetFactoryType() == TaskFactoryType::FAI_GENERAL)
    validation_errors = task->GetFactory().ValidateFAIOZs();

  TCHAR linebreak[3];
  if (linebreaks) {
    linebreak[0] = '\n';
    linebreak[1] = 0;
  } else {
    linebreak[0] = ',';
    linebreak[1] = ' ';
    linebreak[2] = 0;
  }

  if (!task->TaskSize()) {
    StringFormatUnsafe(text, _("Task is empty (%s)"),
                       OrderedTaskFactoryName(task->GetFactoryType()));
  } else {
    if (task->HasTargets())
      StringFormatUnsafe(text, _T("%s%s%s%s%.0f %s%s%s %.0f %s%s%s %.0f %s (%s)"),
                         summary_shape,
                         validation_errors.IsEmpty() ? _T("") : _T(" / "),
                         validation_errors.IsEmpty() ? _T("") : getTaskValidationErrors(validation_errors),
                         linebreak,
                         (double)Units::ToUserDistance(stats.distance_nominal),
                         Units::GetDistanceName(),
                         linebreak,
                         _("max."),
                         (double)Units::ToUserDistance(stats.distance_max_total),
                         Units::GetDistanceName(),
                         linebreak,
                         _("min."),
                         (double)Units::ToUserDistance(stats.distance_min),
                         Units::GetDistanceName(),
                         OrderedTaskFactoryName(task->GetFactoryType()));
    else
      StringFormatUnsafe(text, _T("%s%s%s%s%s %.0f %s (%s)"),
                         summary_shape,
                         validation_errors.IsEmpty() ? _T("") : _T(" / "),
                         validation_errors.IsEmpty() ? _T("") : getTaskValidationErrors(validation_errors),
                         linebreak,
                         _("dist."),
                         (double)Units::ToUserDistance(stats.distance_nominal),
                         Units::GetDistanceName(),
                         OrderedTaskFactoryName(task->GetFactoryType()));
  }
}

void
OrderedTaskPointLabel(TaskPointType type, const TCHAR *name,
                      unsigned index, TCHAR* buffer)
{
  switch (type) {
  case TaskPointType::START:
    StringFormatUnsafe(buffer, _T("S: %s"), name);
    break;

  case TaskPointType::AST:
    StringFormatUnsafe(buffer, _T("T%d: %s"), index, name);
    break;

  case TaskPointType::AAT:
    StringFormatUnsafe(buffer, _T("A%d: %s"), index, name);
    break;

  case TaskPointType::FINISH:
    StringFormatUnsafe(buffer, _T("F: %s"), name);
    break;

  default:
    break;
  }
}

void
OrderedTaskPointRadiusLabel(const ObservationZonePoint &ozp, TCHAR* buffer)
{
  switch (ozp.GetShape()) {
  case ObservationZone::Shape::FAI_SECTOR:
    _tcscpy(buffer, _("FAI quadrant"));
    return;

  case ObservationZone::Shape::SECTOR:
  case ObservationZone::Shape::ANNULAR_SECTOR:
    StringFormatUnsafe(buffer,_T("%s - %s: %.1f%s"), _("Sector"), _("Radius"),
                       (double)Units::ToUserDistance(((const SectorZone &)ozp).GetRadius()),
                       Units::GetDistanceName());
    return;

  case ObservationZone::Shape::LINE:
    StringFormatUnsafe(buffer,_T("%s - %s: %.1f%s"), _("Line"), _("Gate width"),
                       (double)Units::ToUserDistance(((const LineSectorZone &)ozp).GetLength()),
                       Units::GetDistanceName());
    return;

  case ObservationZone::Shape::CYLINDER:
    StringFormatUnsafe(buffer,_T("%s - %s: %.1f%s"), _("Cylinder"), _("Radius"),
                       (double)Units::ToUserDistance(((const CylinderZone &)ozp).GetRadius()),
                       Units::GetDistanceName());
    return;

  case ObservationZone::Shape::MAT_CYLINDER:
    _tcscpy(buffer, _("MAT cylinder"));
    return;

  case ObservationZone::Shape::CUSTOM_KEYHOLE:
    StringFormatUnsafe(buffer,_T("%s - %s: %.1f%s"), _("Keyhole"), _("Radius"),
                       (double)Units::ToUserDistance(((const KeyholeZone &)ozp).GetRadius()),
                       Units::GetDistanceName());
    return;

  case ObservationZone::Shape::DAEC_KEYHOLE:
    _tcscpy(buffer, _("DAeC Keyhole"));
    return;

  case ObservationZone::Shape::BGAFIXEDCOURSE:
    _tcscpy(buffer, _("BGA Fixed Course"));
    return;

  case ObservationZone::Shape::BGAENHANCEDOPTION:
    _tcscpy(buffer, _("BGA Enhanced Option"));
    return;

  case ObservationZone::Shape::BGA_START:
    _tcscpy(buffer, _("BGA Start Sector"));
    return;

  case ObservationZone::Shape::SYMMETRIC_QUADRANT:
    _tcscpy(buffer, _("Symmetric quadrant"));
    return;
  }

  gcc_unreachable();
  assert(false);
}

bool
OrderedTaskSave(OrderedTask &task)
{
  TCHAR fname[69] = _T("");
  if (!TextEntryDialog(fname, 64, _("Enter a task name")))
    return false;

  const auto tasks_path = MakeLocalPath(_T("tasks"));

  _tcscat(fname, _T(".tsk"));
  task.SetName(fname);
  SaveTask(AllocatedPath::Build(tasks_path, fname), task);
  return true;
}
